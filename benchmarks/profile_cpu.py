"""CPU profiler entrypoint for TVLCOM.

Runs a synthetic workload and writes a pstats file.

Run (PowerShell):

    python benchmarks\profile_cpu.py
    python -c "import pstats; p=pstats.Stats('benchmarks/profile.pstats'); p.sort_stats('cumtime').print_stats(30)"
"""

from __future__ import annotations

import cProfile
import os
import sys

# Allow running this file directly without installing the package.
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from PYTVLCOM.COM import build_frame, create_string_entry
from PYTVLCOM.RE import TLVParser


def workload(iterations: int = 20000) -> None:
    payload = b"".join(create_string_entry("hello") for _ in range(10))

    def on_frame(_frame_id: int, _entries) -> None:
        return

    parser = TLVParser(on_frame=on_frame, on_error=None)

    for i in range(iterations):
        frame = build_frame(i & 0xFF, payload)
        for b in frame:
            parser.process_byte(b)


def main() -> None:
    cProfile.run("workload()", filename="benchmarks/profile.pstats")


if __name__ == "__main__":
    main()
