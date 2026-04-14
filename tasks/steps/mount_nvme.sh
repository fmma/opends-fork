#!/bin/bash
# Mount NVMe namespace.
set -e

NS=$1
MOUNT=$2

if [ ! -b "$NS" ]; then
	echo "error: $NS is not a block device"
	exit 1
fi

umount -l "$MOUNT" 2>/dev/null || true
mkdir -p "$MOUNT"

if ! mount "$NS" "$MOUNT"; then
	echo "error: failed to mount $NS on $MOUNT"
	echo "dmesg tail:"
	dmesg | tail -5
	exit 1
fi
echo "$NS mounted on $MOUNT"
