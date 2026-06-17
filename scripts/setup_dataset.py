#!/usr/bin/env python3
"""Provision datasets on the target for the thread-scaling benchmark.

With no args, provisions every dataset. With --dataset, just that one.

Only lmcacheish (synthetic) is created here -- generated if missing. The
reference datasets (tiktokish, imagenetish, filesize8gib) must already be
populated by aisio's tasks/setup_dataset.yaml; this errors and points you there
if one is missing.

Usage: scripts/setup_dataset.py [--dataset lmcacheish]
"""

import argparse

from _helpers import ok, run_cijoe

DATASETS = ["lmcacheish", "tiktokish", "imagenetish", "filesize8gib"]

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dataset", choices=DATASETS,
                    help="dataset to provision; omit to do all")
args = parser.parse_args()

datasets = [args.dataset] if args.dataset else DATASETS
run_cijoe("tasks/setup_dataset.yaml", "bind_nvme", "mount", *datasets,
          out="cijoe-output-setup-dataset")
ok(f"setup_dataset {args.dataset or 'all'}")
