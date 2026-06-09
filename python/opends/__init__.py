"""OpenDS Python bindings.

Thin ctypes layer over the OpenDS C ABI. The user-facing surface lives
in _file; ABI declarations live in _cdll. Select a backend with the
OPENDS_BACKEND env var (default "aisio") or point OPENDS_LIBRARY at a
specific .so.
"""

import ctypes as _ctypes

from . import _cdll as _c
from ._file import (
    HostBuffer,
    OpenDSError,
    OpenDSFile,
    _check,
    deregister_buffer,
    register_buffer,
)

__all__ = [
    "OpenDSFile",
    "OpenDSError",
    "HostBuffer",
    "alloc",
    "free",
    "get_version",
    "register_buffer",
    "deregister_buffer",
]


def alloc(size):
    """Allocate a 4096-aligned host buffer owned by the backend."""
    return HostBuffer(size)


def free(buf):
    buf.free()


def get_version():
    major, minor, patch = _ctypes.c_uint(), _ctypes.c_uint(), _ctypes.c_uint()
    _check(
        _c.get_version(
            _ctypes.byref(major), _ctypes.byref(minor), _ctypes.byref(patch)
        )
    )
    return (major.value, minor.value, patch.value)
