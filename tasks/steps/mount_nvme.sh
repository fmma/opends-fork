#!/bin/bash
# Mount NVMe namespace.
set -e

if [ $# -ne 2 ]; then
	echo "usage: mount_nvme.sh NS MOUNT" >&2
	exit 1
fi

NS=$1
MOUNT=$2

if [ ! -b "$NS" ]; then
	echo "error: $NS is not a block device"
	exit 1
fi

if [ "$(blkid -o value -s TYPE "$NS" 2>/dev/null)" != "xfs" ]; then
	echo "error: no xfs filesystem on $NS; run setup_dataset.yaml first" >&2
	exit 1
fi

umount -l "$MOUNT" 2>/dev/null || true
mkdir -p "$MOUNT"

mount "$NS" "$MOUNT"
echo "$NS mounted on $MOUNT"
