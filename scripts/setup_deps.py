#!/usr/bin/env python3
"""Install OpenDS target dependencies on the target.

Builds and installs xNVMe, xal, OpenDS, and fil in dependency order.
xNVMe is required for the aisio backend; xal and fil are required
for the filperf-driven benchmark suite. fil's opends backend links
against installed libopends_aisio, so OpenDS is installed before fil
is built. Assumes the tree has been synced to the target (rsync.py).
"""

from _helpers import ok, run_cijoe

for task in [
    "tasks/setup_xnvme.yaml",
    "tasks/setup_xal.yaml",
    "tasks/setup_opends.yaml",
    "tasks/setup_fil.yaml",
]:
    run_cijoe(task)

ok("setup_deps")
