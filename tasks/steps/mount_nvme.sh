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

if [ "$(blkid -o value -s TYPE "$NS" 2>/dev/null)" != "xfs" ]; then
	echo "no xfs filesystem on $NS; formatting"
	mkfs.xfs -f -q "$NS"
fi
mount "$NS" "$MOUNT"
echo "$NS mounted on $MOUNT"
