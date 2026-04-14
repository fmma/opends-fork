#!/usr/bin/env python3
"""Check C source formatting with clang-format."""

import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
sources = sorted(root.glob("src/*.c")) + sorted(root.glob("include/*.h"))

rc = 0
for src in sources:
    result = subprocess.run(
        ["clang-format", "--dry-run", "--Werror", str(src)],
        capture_output=True, text=True,
    )
    if result.returncode:
        print(result.stderr, end="")
        rc = 1

sys.exit(rc)
