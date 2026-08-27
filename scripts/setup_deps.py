#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Install OpenDS target dependencies on the target.

Builds and installs xNVMe, xal, HOMI, qublk, OpenDS, and fil in
dependency order. xNVMe is required for the aisio backend. HOMI (homid +
libhomic) opens the controller as the multi-process primary and resolves
file extents; it depends on xal and xnvme, so it follows them. qublk
depends on xnvme. OpenDS links libhomic, so HOMI is installed before
OpenDS. Assumes the tree has been synced to the target (rsync.py).

fil is bench-only and links opends_aisio, so it goes last. It once
needed its own xal prefix; it does not any more, since it asks for
xal >= 0.2.0 and the pinned xal satisfies that.
"""

from _helpers import ok, run_cijoe

for task in [
    "tasks/setup_xnvme.yaml",
    "tasks/setup_xal.yaml",
    "tasks/setup_homi.yaml",
    "tasks/setup_qublk.yaml",
    "tasks/setup_opends.yaml",
    "tasks/setup_fil.yaml",
]:
    run_cijoe(task)

ok("setup_deps")
