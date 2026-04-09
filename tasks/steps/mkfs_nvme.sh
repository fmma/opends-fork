#!/bin/bash
# Create XFS filesystem on NVMe namespace.
# Verifies the namespace belongs to the expected BDF first.
set -e

NS=$1
BDF=$2

SYSLINK=$(readlink "/sys/block/$(basename "$NS")" 2>/dev/null) || true
if ! echo "$SYSLINK" | grep -qF "$BDF"; then
	echo "error: $NS does not belong to PCI device $BDF"
	echo "  sysfs link: $SYSLINK"
	echo "  available block devices:"
	ls /sys/block/
	exit 1
fi

umount "$NS" 2>/dev/null || true
echo "creating xfs on $NS"
mkfs.xfs -f "$NS"
