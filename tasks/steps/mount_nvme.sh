#!/bin/bash
# Resolve the NVMe namespace for BDF and mount it at MOUNT.
set -e

if [ $# -ne 2 ]; then
	echo "usage: mount_nvme.sh BDF MOUNT" >&2
	exit 1
fi

BDF=$1
MOUNT=$2
HERE=$(dirname "$0")

NS=$("$HERE/resolve_nvme_ns.sh" "$BDF") || exit 1

if [ ! -b "$NS" ]; then
	echo "error: $NS is not a block device" >&2
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
