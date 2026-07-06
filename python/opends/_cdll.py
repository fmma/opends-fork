# SPDX-License-Identifier: BSD-3-Clause
"""Library loader and ctypes prototypes for the OpenDS C ABI.

ABI churn lives here. Every signature mirrors the headers under
include/; keep them in sync when the C API changes.
"""

import ctypes
import os
from ctypes import (
    c_char_p,
    c_int,
    c_long,
    c_size_t,
    c_ssize_t,
    c_uint,
    c_void_p,
)
from pathlib import Path

OPENDS_SUCCESS = 0
OPENDS_MEMORY_ALREADY_REGISTERED = 5023


class DsError(ctypes.Structure):
    """opends_error_t: {opends_op_error_t err; opends_result_t dev_err;}."""

    _fields_ = [("err", c_int), ("dev_err", c_int)]


BACKEND = os.environ.get("OPENDS_BACKEND", "aisio")


def _candidate_paths():
    soname = "libopends_%s.so" % BACKEND
    explicit = os.environ.get("OPENDS_LIBRARY")
    if explicit:
        yield explicit
    repo = Path(__file__).resolve().parents[2]
    yield str(repo / "build" / soname)
    yield soname


def _load():
    last = None
    for path in _candidate_paths():
        try:
            return ctypes.CDLL(path)
        except OSError as exc:
            last = exc
    raise OSError(
        "could not load libopends_%s.so; set OPENDS_LIBRARY or build the "
        "C library first (meson compile -C build). Last error: %s"
        % (BACKEND, last)
    )


_lib = _load()


def _decl(name, restype, argtypes):
    fn = getattr(_lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


driver_open = _decl("opends_driver_open", DsError, [])
driver_close = _decl("opends_driver_close", DsError, [])
use_count = _decl("opends_use_count", c_long, [])
get_version = _decl(
    "opends_get_version",
    DsError,
    [ctypes.POINTER(c_uint), ctypes.POINTER(c_uint), ctypes.POINTER(c_uint)],
)

handle_register = _decl(
    "opends_handle_register", DsError, [ctypes.POINTER(c_void_p), c_int]
)
handle_deregister = _decl("opends_handle_deregister", None, [c_void_p])

alloc = _decl("opends_alloc", c_void_p, [c_size_t])
free = _decl("opends_free", None, [c_void_p])

buf_register = _decl(
    "opends_buf_register", DsError, [c_void_p, c_size_t, c_int]
)
buf_deregister = _decl("opends_buf_deregister", DsError, [c_void_p])

read = _decl(
    "opends_read", c_ssize_t, [c_void_p, c_void_p, c_size_t, c_long, c_long]
)
write = _decl(
    "opends_write", c_ssize_t, [c_void_p, c_void_p, c_size_t, c_long, c_long]
)

op_status_error = _decl("opends_op_status_error", c_char_p, [c_int])


# ---------------------------------------------------------------------------
# Driver properties, batch I/O, and stream-async I/O. Structs and enums mirror
# opends.h; keep them in sync when the C ABI changes.
# ---------------------------------------------------------------------------

# opends_opcode_t
OPENDS_READ = 0
OPENDS_WRITE = 1

# opends_status_t (bit flags)
OPENDS_WAITING = 0x000001
OPENDS_PENDING = 0x000002
OPENDS_INVALID = 0x000004
OPENDS_CANCELED = 0x000008
OPENDS_COMPLETE = 0x000010
OPENDS_TIMEOUT = 0x000020
OPENDS_FAILED = 0x000040

# opends_batch_mode_t
OPENDS_BATCH = 1


class DsProps(ctypes.Structure):
    """opends_drv_props_t."""

    _fields_ = [
        ("major_version", c_uint),
        ("minor_version", c_uint),
        ("max_direct_io_size", c_size_t),
        ("max_batch_io_size", c_uint),
        ("max_batch_io_timeout_msecs", c_uint),
    ]


class _IoParamsBatch(ctypes.Structure):
    _fields_ = [
        ("dev_ptr_base", c_void_p),
        ("file_offset", c_long),
        ("dev_ptr_offset", c_long),
        ("size", c_size_t),
    ]


class _IoParamsU(ctypes.Union):
    _fields_ = [("batch", _IoParamsBatch)]


class IoParams(ctypes.Structure):
    """opends_io_params_t."""

    _fields_ = [
        ("mode", c_int),
        ("u", _IoParamsU),
        ("fh", c_void_p),
        ("opcode", c_int),
        ("cookie", c_void_p),
    ]


class IoEvents(ctypes.Structure):
    """opends_io_events_t."""

    _fields_ = [
        ("cookie", c_void_p),
        ("status", c_int),
        ("ret", c_size_t),
    ]


class Timespec(ctypes.Structure):
    """struct timespec."""

    _fields_ = [("tv_sec", c_long), ("tv_nsec", c_long)]


driver_get_properties = _decl(
    "opends_driver_get_properties", DsError, [ctypes.POINTER(DsProps)]
)
driver_set_max_direct_io_size = _decl(
    "opends_driver_set_max_direct_io_size", DsError, [c_size_t]
)

batch_io_setup = _decl(
    "opends_batch_io_setup", DsError, [ctypes.POINTER(c_void_p), c_uint]
)
batch_io_submit = _decl(
    "opends_batch_io_submit",
    DsError,
    [c_void_p, c_uint, ctypes.POINTER(IoParams), c_uint],
)
batch_io_get_status = _decl(
    "opends_batch_io_get_status",
    DsError,
    [
        c_void_p,
        c_uint,
        ctypes.POINTER(c_uint),
        ctypes.POINTER(IoEvents),
        ctypes.POINTER(Timespec),
    ],
)
batch_io_cancel = _decl("opends_batch_io_cancel", DsError, [c_void_p])
batch_io_destroy = _decl("opends_batch_io_destroy", None, [c_void_p])

# opends_read_async / opends_write_async share this signature.
_ASYNC_ARGS = [
    c_void_p,                   # fh
    c_void_p,                   # buf_base
    ctypes.POINTER(c_size_t),   # size_p
    ctypes.POINTER(c_long),     # file_offset_p (off_t)
    ctypes.POINTER(c_long),     # buf_offset_p  (off_t)
    ctypes.POINTER(c_ssize_t),  # bytes_p
    c_void_p,                   # stream
]
read_async = _decl("opends_read_async", DsError, _ASYNC_ARGS)
write_async = _decl("opends_write_async", DsError, _ASYNC_ARGS)
stream_register = _decl("opends_stream_register", DsError, [c_void_p, c_uint])
stream_deregister = _decl("opends_stream_deregister", DsError, [c_void_p])
