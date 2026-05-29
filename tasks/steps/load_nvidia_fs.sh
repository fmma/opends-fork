#!/bin/bash
# Ensure nvidia-fs.ko is loaded so cuFile uses the real P2P path
# (NVMe -> GPU BAR1 via nvidia-fs) instead of CPU-staged compat mode.
# With allow_compat_mode=false in /etc/cufile.json, cuFile errors out
# with 5001 (driver not loaded) if this module is missing, which is the
# loud-failure mode we want for benches.
set -e

if lsmod | grep -q '^nvidia_fs '; then
	echo "nvidia_fs already loaded"
	exit 0
fi

modprobe nvidia_fs
echo "nvidia_fs loaded"
