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

DS_FILE_SUCCESS = 0
DS_FILE_MEMORY_ALREADY_REGISTERED = 5023


class DsError(ctypes.Structure):
    """ds_file_error_t: {ds_file_op_error_t err; ds_result_t dev_err;}."""

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


driver_open = _decl("ds_file_driver_open", DsError, [])
driver_close = _decl("ds_file_driver_close", DsError, [])
use_count = _decl("ds_file_use_count", c_long, [])
get_version = _decl(
    "ds_file_get_version",
    DsError,
    [ctypes.POINTER(c_uint), ctypes.POINTER(c_uint), ctypes.POINTER(c_uint)],
)

handle_register = _decl(
    "ds_file_handle_register", DsError, [ctypes.POINTER(c_void_p), c_int]
)
handle_deregister = _decl("ds_file_handle_deregister", None, [c_void_p])

alloc = _decl("ds_file_alloc", c_void_p, [c_size_t])
free = _decl("ds_file_free", None, [c_void_p])

buf_register = _decl(
    "ds_file_buf_register", DsError, [c_void_p, c_size_t, c_int]
)
buf_deregister = _decl("ds_file_buf_deregister", DsError, [c_void_p])

read = _decl(
    "ds_file_read", c_ssize_t, [c_void_p, c_void_p, c_size_t, c_long, c_long]
)
write = _decl(
    "ds_file_write", c_ssize_t, [c_void_p, c_void_p, c_size_t, c_long, c_long]
)

op_status_error = _decl("ds_file_op_status_error", c_char_p, [c_int])
