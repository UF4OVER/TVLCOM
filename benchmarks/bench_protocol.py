"""Micro-benchmarks for TVLCOM protocol primitives.

This script measures CPU time for:
- CRC16 calculation
- TLV parsing
- frame build + stream parse
- memory peak during TLV parsing

Run (PowerShell):

    python benchmarks\bench_protocol.py
"""

from __future__ import annotations

import os
import sys
import time
import tracemalloc

# Allow running this file directly without installing the package.
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from PYTVLCOM.COM import build_frame, crc16_ccitt, create_string_entry
from PYTVLCOM.RE import TLVParser, parse_tlvs


def _now() -> float:
    return time.perf_counter()


def bench_crc(iterations: int = 20000) -> dict[str, float]:
    data = b"1234567890" * 32
    t0 = _now()
    s = 0
    for _ in range(iterations):
        s ^= crc16_ccitt(data)
    t1 = _now()
    return {"crc_time_s": t1 - t0, "crc_dummy": float(s & 0xFFFF)}


def bench_parse_tlvs(iterations: int = 20000) -> dict[str, float]:
    payload = b"".join(create_string_entry("hello") for _ in range(10))
    t0 = _now()
    cnt = 0
    for _ in range(iterations):
        cnt += len(parse_tlvs(payload))
    t1 = _now()
    return {"parse_tlvs_time_s": t1 - t0, "entries": float(cnt)}


def bench_frame_roundtrip(iterations: int = 5000) -> dict[str, float]:
    payload = b"".join(create_string_entry("hello") for _ in range(10))

    got = 0

    def on_frame(_frame_id: int, entries) -> None:
        nonlocal got
        got += len(entries)

    parser = TLVParser(on_frame=on_frame, on_error=None)

    t0 = _now()
    for i in range(iterations):
        frame = build_frame(i & 0xFF, payload)
        for b in frame:
            parser.process_byte(b)
    t1 = _now()

    return {"frame_roundtrip_time_s": t1 - t0, "entries": float(got)}


def bench_memory() -> dict[str, float]:
    payload = b"".join(create_string_entry("hello") for _ in range(100))

    tracemalloc.start()
    t0 = _now()
    for _ in range(2000):
        _ = parse_tlvs(payload)
    t1 = _now()
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()

    return {
        "mem_time_s": t1 - t0,
        "mem_current_kb": current / 1024.0,
        "mem_peak_kb": peak / 1024.0,
    }


def main() -> None:
    print("== TVLCOM micro-benchmarks ==")
    print(bench_crc())
    print(bench_parse_tlvs())
    print(bench_frame_roundtrip())
    print(bench_memory())


if __name__ == "__main__":
    main()
