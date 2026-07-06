# SPDX-License-Identifier: BSD-3-Clause
"""Tests for the batch, stream-async, and driver-property bindings.

Runs against the ref backend (no GPU): it executes batch ops on submit and
async reads immediately, so results and data are verifiable synchronously.
"""

import ctypes
import os
import tempfile

import opends
import opends._cdll as cdll


def _payload(n):
    return bytes((i * 31 + 7) % 251 for i in range(n))


def _write_file(payload):
    fd, path = tempfile.mkstemp()
    os.close(fd)
    src = opends.alloc(len(payload))
    ctypes.memmove(src.as_ctypes(), payload, len(payload))
    with opends.OpenDSFile(path, "w") as f:
        assert f.write(src, size=len(payload)) == len(payload)
    return path


def test_driver_properties():
    p = opends.driver_properties()
    for k in ("major_version", "minor_version", "max_direct_io_size",
              "max_batch_io_size", "max_batch_io_timeout_msecs"):
        assert k in p and isinstance(p[k], int)
    assert (p["major_version"], p["minor_version"]) == (1, 0)


def test_set_max_direct_io_size():
    opends.set_max_direct_io_size(1 << 20)  # ref accepts any value


def test_async_read_roundtrip():
    n = 4096
    payload = _payload(n)
    path = _write_file(payload)
    try:
        dst = opends.alloc(n)
        opends.register_stream(0)
        try:
            with opends.OpenDSFile(path, "r") as f:
                op = f.read_async(dst, size=n, stream=0)
                assert op.result == n  # ref completes on submit
        finally:
            opends.deregister_stream(0)
        assert bytes(dst.as_ctypes()) == payload
    finally:
        os.unlink(path)


def test_batch_read_scatter():
    n = 4096
    half = n // 2
    payload = _payload(n)
    path = _write_file(payload)
    try:
        dst = opends.alloc(n)
        with opends.OpenDSFile(path, "r") as f, opends.BatchIO(2) as batch:
            batch.submit([
                f.read_op(dst, size=half, file_offset=0, dev_offset=0,
                          cookie=11),
                f.read_op(dst, size=half, file_offset=half, dev_offset=half,
                          cookie=22),
            ])
            events = batch.get_status(min_nr=2)
        assert len(events) == 2
        assert all(status == cdll.OPENDS_COMPLETE for _, status, _r in events)
        assert sorted(cookie for cookie, _, _ in events) == [11, 22]
        assert all(ret == half for _, _, ret in events)
        assert bytes(dst.as_ctypes()) == payload
    finally:
        os.unlink(path)


def test_batch_write_then_read():
    n = 2048
    payload = _payload(n)
    fd, path = tempfile.mkstemp()
    os.close(fd)
    try:
        src = opends.alloc(n)
        ctypes.memmove(src.as_ctypes(), payload, n)
        with opends.OpenDSFile(path, "w") as f, opends.BatchIO(1) as batch:
            batch.submit([f.write_op(src, size=n, cookie=7)])
            events = batch.get_status(min_nr=1)
        assert events == [(7, cdll.OPENDS_COMPLETE, n)]

        dst = opends.alloc(n)
        with opends.OpenDSFile(path, "r") as f:
            assert f.read(dst, size=n) == n
        assert bytes(dst.as_ctypes()) == payload
    finally:
        os.unlink(path)


if __name__ == "__main__":
    test_driver_properties()
    test_set_max_direct_io_size()
    test_async_read_roundtrip()
    test_batch_read_scatter()
    test_batch_write_then_read()
    print("all ok")
