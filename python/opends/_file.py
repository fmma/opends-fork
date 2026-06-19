"""User-facing OpenDS classes: OpenDSFile and HostBuffer.

read/write are synchronous and return the byte count, mirroring the
opends_read/opends_write C surface. Buffer arguments follow the
cuFile convention (base pointer plus dev_offset) so consumers written
against GDS port with minimal change.
"""

import atexit
import ctypes
import math
import os
import threading

from . import _cdll as _c

# ---------------------------------------------------------------------------
# CUDA context preservation. The aisio (upcie-cuda) backend creates its own
# CUDA context and leaves it current across native calls. A host framework
# (e.g. PyTorch/vLLM) creates its stream and event handles in its own primary
# context; if our calls leave a different context current, those handles fault
# with CUDA "invalid resource handle". Save the caller's current context and
# restore it around every native call that may enter the CUDA backend. No-op
# when libcuda is unavailable (CPU/ref backend) or no context is current.
# ---------------------------------------------------------------------------
try:
    _libcuda = ctypes.CDLL("libcuda.so")
    # Set explicit signatures: default int marshalling happens to work for
    # pointer-sized values on x86-64 but must not be assumed.
    _libcuda.cuCtxGetCurrent.restype = ctypes.c_int
    _libcuda.cuCtxGetCurrent.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    _libcuda.cuCtxSetCurrent.restype = ctypes.c_int
    _libcuda.cuCtxSetCurrent.argtypes = [ctypes.c_void_p]
except OSError:
    _libcuda = None


class _preserve_cuda_context:
    """Save/restore the caller's current CUDA context around native calls.

    No-op when libcuda is unavailable (CPU/ref backend) or no context is
    current. Only preserves an existing context; never calls cuInit and
    never creates one. Per-thread-correct: cuCtxGet/SetCurrent act on the
    calling thread.
    """

    def __enter__(self):
        self._ctx = None
        if _libcuda is not None:
            c = ctypes.c_void_p()
            if _libcuda.cuCtxGetCurrent(ctypes.byref(c)) == 0 and c.value:
                self._ctx = c
        return self

    def __exit__(self, *exc):
        if self._ctx is not None:
            _libcuda.cuCtxSetCurrent(self._ctx)
        return False


class OpenDSError(Exception):
    def __init__(self, code, dev_err=0):
        self.code = int(code)
        self.dev_err = int(dev_err)
        msg = _c.op_status_error(self.code)
        msg = msg.decode() if msg else "unknown opends error"
        super().__init__("%s (code %d, dev_err %d)" % (msg, self.code, self.dev_err))


def _check(err):
    if err.err != _c.OPENDS_SUCCESS:
        raise OpenDSError(err.err, err.dev_err)


def _io_result(ret):
    if ret < 0:
        raise OpenDSError(-ret)
    return ret


# ---------------------------------------------------------------------------
# Driver lifecycle. The C driver is a process-global singleton; reference
# count it so open files and pinned registrations share one open/close.
# ---------------------------------------------------------------------------

_lock = threading.Lock()
_driver_refs = 0
# Pointers pinned by register_buffer. Each holds a driver reference so the
# registration outlives individual file opens, matching cuFileBufRegister.
_pinned = set()


def _ensure_driver():
    global _driver_refs
    with _lock:
        if _driver_refs == 0:
            _check(_c.driver_open())
        _driver_refs += 1


def _release_driver():
    global _driver_refs
    with _lock:
        if _driver_refs == 0:
            return
        _driver_refs -= 1
        if _driver_refs == 0:
            _pinned.clear()
            _registry.clear()
            _check(_c.driver_close())


@atexit.register
def _shutdown():
    global _driver_refs
    with _lock:
        if _driver_refs > 0:
            _pinned.clear()
            _registry.clear()
            _c.driver_close()
            _driver_refs = 0


# ---------------------------------------------------------------------------
# Buffer registration cache. Externally allocated buffers must be registered
# before I/O (required by the aisio/gds backends; a no-op effect on ref).
# Registration is keyed by base pointer and lazily established on first use.
# ---------------------------------------------------------------------------


class _Registry:
    def __init__(self):
        self._regs = {}
        self._lock = threading.Lock()

    def ensure(self, ptr, size):
        with self._lock:
            if ptr in self._regs:
                return
            if size is None:
                raise ValueError(
                    "bare pointer 0x%x is not registered; call "
                    "opends.register_buffer(ptr, size) on the base buffer "
                    "before using it for I/O" % ptr
                )
            err = _c.buf_register(ptr, size, 0)
            if err.err not in (
                _c.OPENDS_SUCCESS,
                _c.OPENDS_MEMORY_ALREADY_REGISTERED,
            ):
                raise OpenDSError(err.err, err.dev_err)
            self._regs[ptr] = size

    def drop(self, ptr):
        with self._lock:
            if self._regs.pop(ptr, None) is not None:
                _c.buf_deregister(ptr)

    def clear(self):
        with self._lock:
            for ptr in list(self._regs):
                _c.buf_deregister(ptr)
            self._regs.clear()


_registry = _Registry()


def register_buffer(buf, size=None):
    """Register a buffer for DMA before I/O.

    Structured buffers (HostBuffer, cupy/numpy/torch arrays) are
    registered on first use, so this is only needed for the cuFile
    pattern of registering one large base allocation and then indexing
    into it with a bare device pointer and dev_offset. For a bare
    pointer (ctypes.c_void_p or int) size is mandatory; for a structured
    buffer it defaults to the buffer's own extent.

    The registration pins the driver open until deregister_buffer, so it
    persists across OpenDSFile open/close cycles.
    """
    ptr, nbytes = _buffer_view(buf)
    if size is None:
        if nbytes is None:
            raise ValueError("size is required to register a bare pointer")
        size = nbytes
    with _lock:
        first = ptr not in _pinned
    with _preserve_cuda_context():
        if first:
            _ensure_driver()
        try:
            _registry.ensure(ptr, int(size))
        except Exception:
            if first:
                _release_driver()
            raise
    with _lock:
        _pinned.add(ptr)


def deregister_buffer(buf):
    """Drop a registration established by register_buffer or first use."""
    ptr, _ = _buffer_view(buf)
    with _lock:
        pinned = ptr in _pinned
        _pinned.discard(ptr)
    with _preserve_cuda_context():
        _registry.drop(ptr)
        if pinned:
            _release_driver()


# ---------------------------------------------------------------------------
# HostBuffer: an opends_alloc-backed, 4096-aligned host allocation. Useful on
# the ref backend and as a DMA-able staging buffer without pulling in numpy.
# ---------------------------------------------------------------------------


class HostBuffer:
    def __init__(self, size):
        ptr = _c.alloc(size)
        if not ptr:
            raise MemoryError("opends_alloc(%d) failed" % size)
        self._ptr = int(ptr)
        self._size = int(size)

    @property
    def ptr(self):
        return self._ptr

    @property
    def nbytes(self):
        return self._size

    def __len__(self):
        return self._size

    def as_ctypes(self):
        return (ctypes.c_char * self._size).from_address(self._ptr)

    def free(self):
        if self._ptr:
            deregister_buffer(self._ptr)
            _c.free(self._ptr)
            self._ptr = 0

    def __del__(self):
        try:
            self.free()
        except Exception:
            pass


def _nbytes_from_iface(iface):
    itemsize = int(iface["typestr"][2:])
    return itemsize * math.prod(iface["shape"])


def _buffer_view(buf):
    """Return (base_ptr:int, nbytes:int|None) for a buffer-like object.

    A bare device pointer (ctypes.c_void_p or int address, the cuFile
    calling convention) has no discoverable extent, so nbytes is None;
    such a buffer must be registered up front and given an explicit size
    at I/O time.
    """
    if isinstance(buf, HostBuffer):
        return buf.ptr, buf.nbytes
    if isinstance(buf, ctypes.c_void_p):
        return int(buf.value or 0), None
    if isinstance(buf, int):
        return buf, None
    cai = getattr(buf, "__cuda_array_interface__", None)
    if cai is not None:
        return int(cai["data"][0]), _nbytes_from_iface(cai)
    ai = getattr(buf, "__array_interface__", None)
    if ai is not None:
        return int(ai["data"][0]), _nbytes_from_iface(ai)
    if hasattr(buf, "data_ptr"):  # torch.Tensor (CPU)
        nbytes = getattr(buf, "nbytes", None)
        if nbytes is None:
            nbytes = buf.numel() * buf.element_size()
        return int(buf.data_ptr()), int(nbytes)
    mv = memoryview(buf)
    arr = (ctypes.c_char * mv.nbytes).from_buffer(buf)
    return ctypes.addressof(arr), mv.nbytes


_FLAGS = {
    "r": os.O_RDONLY,
    "w": os.O_WRONLY | os.O_CREAT | os.O_TRUNC,
    "rw": os.O_RDWR | os.O_CREAT,
    "r+": os.O_RDWR,
    "a": os.O_WRONLY | os.O_CREAT | os.O_APPEND,
}

# Backends that need the file descriptor opened O_DIRECT by default. gds
# drives cuFile through the fd and requires it; aisio uses the fd only for
# FIEMAP extent lookup (data moves by userspace DMA); ref is buffered.
_DIRECT_BACKENDS = {"gds"}


class OpenDSFile:
    def __init__(self, path, flags="r", *, use_direct_io=None, mode=0o644):
        if flags not in _FLAGS:
            raise ValueError("unsupported flags %r" % flags)
        oflags = _FLAGS[flags]
        if use_direct_io is None:
            use_direct_io = _c.BACKEND in _DIRECT_BACKENDS
        if use_direct_io:
            oflags |= os.O_DIRECT
        self._fh = None
        self._fd = os.open(path, oflags, mode)
        try:
            with _preserve_cuda_context():
                _ensure_driver()
                fh = ctypes.c_void_p()
                _check(_c.handle_register(ctypes.byref(fh), self._fd))
            self._fh = fh
        except Exception:
            os.close(self._fd)
            self._fd = -1
            raise

    def _submit(self, c_fn, buf, size, file_offset, dev_offset):
        ptr, nbytes = _buffer_view(buf)
        if size is None:
            if nbytes is None:
                raise ValueError("size is required for a bare pointer")
            size = nbytes - dev_offset
        with _preserve_cuda_context():
            _registry.ensure(ptr, nbytes)
            return _io_result(c_fn(self._fh, ptr, size, file_offset, dev_offset))

    def read(self, buf, size=None, file_offset=0, dev_offset=0):
        return self._submit(_c.read, buf, size, file_offset, dev_offset)

    def write(self, buf, size=None, file_offset=0, dev_offset=0):
        return self._submit(_c.write, buf, size, file_offset, dev_offset)

    def close(self):
        if self._fh is not None:
            with _preserve_cuda_context():
                _c.handle_deregister(self._fh)
                self._fh = None
                _release_driver()
        if self._fd >= 0:
            os.close(self._fd)
            self._fd = -1

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
