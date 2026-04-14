#!/usr/bin/env python3
"""Generate expected read output from the test file.

Reads the same regions as the test binaries (from read_cases.txt)
and writes output in the same format. Used as a baseline for verifying
backend correctness.
"""
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CASES_FILE = REPO_ROOT / "tests" / "read_cases.txt"


def load_cases(path):
    cases = []
    with open(path) as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            offset, size = line.split()
            cases.append((int(offset), int(size)))
    return cases


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input_file> <output_file>",
              file=sys.stderr)
        sys.exit(1)

    cases = load_cases(CASES_FILE)

    with open(sys.argv[1], "rb") as f:
        data = f.read()

    os.makedirs(os.path.dirname(sys.argv[2]), exist_ok=True)

    with open(sys.argv[2], "wb") as out:
        for i, (offset, size) in enumerate(cases):
            chunk = data[offset:offset + size]
            got = len(chunk)
            out.write(f"CASE {i} off={offset} sz={size} got={got}\n".encode())
            out.write(chunk)
            out.write(b"\n")


if __name__ == "__main__":
    main()
