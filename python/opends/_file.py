# SPDX-License-Identifier: BSD-3-Clause
"""User-facing OpenDS classes: OpenDSFile and HostBuffer.

read/write are synchronous and return the byte count, mirroring the
opends_read/opends_write C surface. Buffer arguments follow the
cuFile convention (base pointer plus offset) so consumers written
against cuFile port with minimal change.
"""

import atexit
import ctypes
import math
import os
import signal
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

# Reentrant so a signal-driven _cleanup can re-acquire while the same thread
# already holds the lock, instead of deadlocking.
_lock = threading.RLock()
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
    _install_signal_handlers()


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


# Canonical idempotent teardown: drop registrations and close the driver,
# releasing the GPU dma-buf. Kept module-public under this exact name because
# frameworks call opends._file._cleanup() from their own SIGTERM handler (they
# shadow any handler we install, and their graceful shutdown may never run the
# LMCache close that would deregister us). Renaming it silently leaks the buffer.
#
# _cleaning guards against signal reentrancy: a second SIGTERM/SIGINT arriving
# while _cleanup is inside the slow buf_deregister re-enters it on the same
# thread, where _registry's non-reentrant lock would self-deadlock and
# driver_close would never run. A reentrant call is a no-op so the first
# completes; reset in finally so a later driver lifecycle can clean up too.
_cleaning = False


@atexit.register
def _cleanup():
    global _driver_refs, _cleaning
    if _cleaning:
        return
    _cleaning = True
    try:
        with _lock:
            if _driver_refs > 0:
                _pinned.clear()
                _registry.clear()
                _c.driver_close()
                _driver_refs = 0
    finally:
        _cleaning = False


# ---------------------------------------------------------------------------
# Backstop for standalone consumers: atexit does not run on signal death, so a
# SIGTERM/SIGINT with no handler skips driver_close and leaks the GPU dma-buf.
# Install a handler that runs _cleanup and chains to the prior one. A framework
# that installs its own SIGTERM handler after import (e.g. vLLM) shadows this
# and calls opends._file._cleanup() directly instead.
# ---------------------------------------------------------------------------

_CLEANUP_SIGNALS = (signal.SIGTERM, signal.SIGINT)
_prev_handlers = {}


def _signal_cleanup(signum, frame):
    _cleanup()
    prev = _prev_handlers.get(signum, signal.SIG_DFL)
    if prev is None:  # handler installed outside Python; treat as default
        prev = signal.SIG_DFL
    if callable(prev):
        prev(signum, frame)
    else:
        # SIG_DFL / SIG_IGN: restore it and re-raise so the original
        # disposition (terminate for SIGTERM) still applies.
        signal.signal(signum, prev)
        os.kill(os.getpid(), signum)


def _install_signal_handlers():
    """Route SIGTERM/SIGINT through _signal_cleanup, chaining to whatever was
    installed before. Idempotent (skips signals we already own, so it never
    chains to itself). A no-op off the main thread, where signal.signal raises;
    atexit still covers a clean exit there.
    """
    for sig in _CLEANUP_SIGNALS:
        try:
            cur = signal.getsignal(sig)
            if cur is _signal_cleanup:
                continue
            signal.signal(sig, _signal_cleanup)
            _prev_handlers[sig] = cur
        except (ValueError, OSError):
            pass


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
    if hasattr(buf, "data_ptr"):
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


# ---------------------------------------------------------------------------
# Driver properties.
# ---------------------------------------------------------------------------


def driver_properties():
    """Return the backend driver properties as a dict."""
    _ensure_driver()
    try:
        p = _c.DsProps()
        _check(_c.driver_get_properties(ctypes.byref(p)))
        return {
            "major_version": p.major_version,
            "minor_version": p.minor_version,
            "max_direct_io_size": p.max_direct_io_size,
            "max_batch_io_size": p.max_batch_io_size,
            "max_batch_io_timeout_msecs": p.max_batch_io_timeout_msecs,
        }
    finally:
        _release_driver()


def set_max_direct_io_size(size):
    """Set the driver's max direct-I/O chunk size in bytes."""
    _ensure_driver()
    try:
        _check(_c.driver_set_max_direct_io_size(int(size)))
    finally:
        _release_driver()


# ---------------------------------------------------------------------------
# Stream-based async I/O. The stream is an opaque accelerator stream handle
# (e.g. a CUDA stream), passed through to the backend untouched; register it
# before submitting async ops on it, deregister when done. The ref backend
# ignores it and completes synchronously.
# ---------------------------------------------------------------------------


def _stream_handle(stream):
    """Coerce an accelerator stream to a ctypes void* handle.

    The handle is opaque here and forwarded to the backend as-is (e.g. a
    CUDA stream). Accepts a raw int handle, a ctypes pointer, or an object
    exposing the handle as .cuda_stream (torch) or .ptr (cupy). None is the
    default (0) stream.
    """
    if stream is None or isinstance(stream, ctypes.c_void_p):
        return stream
    if isinstance(stream, int):
        return ctypes.c_void_p(stream)
    for attr in ("cuda_stream", "ptr"):
        h = getattr(stream, attr, None)
        if h is not None:
            return ctypes.c_void_p(int(h))
    raise TypeError(
        "stream must be an int handle, a ctypes pointer, or a torch/cupy "
        "stream; got %s" % type(stream).__name__
    )


def register_stream(stream, flags=0):
    """Register an accelerator stream for async I/O (opends_stream_register)."""
    _check(_c.stream_register(_stream_handle(stream), flags))


def deregister_stream(stream):
    """Deregister a previously registered stream."""
    _check(_c.stream_deregister(_stream_handle(stream)))


class AsyncOp:
    """A submitted async I/O operation.

    The backend reads the size/offset cells at stream-execution time, so keep
    this object alive until the stream it was submitted on has synchronized,
    then read `.result` for the transferred byte count.
    """

    __slots__ = ("_size", "_foff", "_boff", "_ret")

    def __init__(self, size, file_offset, buf_offset):
        self._size = ctypes.c_size_t(size)
        self._foff = ctypes.c_long(file_offset)
        self._boff = ctypes.c_long(buf_offset)
        self._ret = ctypes.c_ssize_t(0)

    @property
    def result(self):
        r = self._ret.value
        if r < 0:
            raise OpenDSError(-r)
        return r


class BatchIO:
    """Batch I/O queue: submit multiple ops and poll for completion.

    Build ops with OpenDSFile.read_op / write_op, submit them, then
    get_status for the events. Usable as a context manager.
    """

    def __init__(self, nr):
        self._h = ctypes.c_void_p()
        _check(_c.batch_io_setup(ctypes.byref(self._h), int(nr)))
        self._capacity = int(nr)

    def submit(self, ops, flags=0):
        n = len(ops)
        arr = (_c.IoParams * n)(*ops)
        with _preserve_cuda_context():
            _check(_c.batch_io_submit(self._h, n, arr, flags))

    def get_status(self, min_nr=1, max_nr=None, timeout=None):
        """Poll for at least min_nr completions. Returns a list of
        (cookie:int, status:int, bytes:int)."""
        if max_nr is None:
            max_nr = self._capacity
        n = ctypes.c_uint(max_nr)
        events = (_c.IoEvents * max_nr)()
        ts_ref = None
        if timeout is not None:
            sec = int(timeout)
            ts = _c.Timespec(sec, int((timeout - sec) * 1e9))
            ts_ref = ctypes.byref(ts)
        _check(_c.batch_io_get_status(
            self._h, min_nr, ctypes.byref(n), events, ts_ref))
        return [(events[i].cookie or 0, events[i].status, events[i].ret)
                for i in range(n.value)]

    def cancel(self):
        _check(_c.batch_io_cancel(self._h))

    def destroy(self):
        if self._h:
            _c.batch_io_destroy(self._h)
            self._h = ctypes.c_void_p()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.destroy()
        return False


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

    def _resolve_size(self, nbytes, size, dev_offset):
        if size is None:
            if nbytes is None:
                raise ValueError("size is required for a bare pointer")
            size = nbytes - dev_offset
        return size

    def _submit_async(self, c_fn, buf, size, file_offset, dev_offset, stream):
        ptr, nbytes = _buffer_view(buf)
        size = self._resolve_size(nbytes, size, dev_offset)
        op = AsyncOp(size, file_offset, dev_offset)
        with _preserve_cuda_context():
            _registry.ensure(ptr, nbytes)
            _check(c_fn(
                self._fh, ptr, ctypes.byref(op._size), ctypes.byref(op._foff),
                ctypes.byref(op._boff), ctypes.byref(op._ret),
                _stream_handle(stream)))
        return op

    def read_async(self, buf, size=None, file_offset=0, dev_offset=0,
                   stream=None):
        """Submit an async read on `stream`. Returns an AsyncOp; read its
        `.result` once the stream has synchronized."""
        return self._submit_async(
            _c.read_async, buf, size, file_offset, dev_offset, stream)

    def write_async(self, buf, size=None, file_offset=0, dev_offset=0,
                    stream=None):
        """Submit an async write on `stream`. Returns an AsyncOp."""
        return self._submit_async(
            _c.write_async, buf, size, file_offset, dev_offset, stream)

    def _io_op(self, opcode, buf, size, file_offset, dev_offset, cookie):
        ptr, nbytes = _buffer_view(buf)
        size = self._resolve_size(nbytes, size, dev_offset)
        _registry.ensure(ptr, nbytes)
        p = _c.IoParams()
        p.mode = _c.OPENDS_BATCH
        p.u.batch.dev_ptr_base = ptr
        p.u.batch.file_offset = file_offset
        p.u.batch.dev_ptr_offset = dev_offset
        p.u.batch.size = size
        fh = self._fh
        p.fh = fh.value if isinstance(fh, ctypes.c_void_p) else fh
        p.opcode = opcode
        p.cookie = cookie
        return p

    def read_op(self, buf, size=None, file_offset=0, dev_offset=0, cookie=0):
        """Build a batch read op for this file; submit it via BatchIO."""
        return self._io_op(
            _c.OPENDS_READ, buf, size, file_offset, dev_offset, cookie)

    def write_op(self, buf, size=None, file_offset=0, dev_offset=0, cookie=0):
        """Build a batch write op for this file; submit it via BatchIO."""
        return self._io_op(
            _c.OPENDS_WRITE, buf, size, file_offset, dev_offset, cookie)

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
