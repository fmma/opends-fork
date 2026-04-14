#!/usr/bin/env python3
"""Fix C source formatting with clang-format."""

import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent.parent
sources = sorted(root.glob("src/*.c")) + sorted(root.glob("include/*.h"))

for src in sources:
    subprocess.run(["clang-format", "-i", str(src)])
