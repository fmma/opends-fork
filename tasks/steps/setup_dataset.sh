#!/bin/bash
# Provision one dataset for the thread-scaling benchmark on the mounted XFS.
#
# lmcacheish is synthetic and generated here (idempotent). The reference datasets
# (tiktokish, imagenetish, filesize8gib) are produced by aisio's
# tasks/setup_dataset.yaml; this errors and points there if one is missing.
#
# args: <dataset> <mount_point>
set -euo pipefail

DS=${1:?dataset}
MNT=${2:?mount_point}
DIR="$MNT/$DS"

if [ "$DS" = lmcacheish ]; then
	"$(dirname "$0")/gen_lmcache_dataset.sh" "$DIR" 1820 14680064
elif [ -z "$(find "$DIR" -type f -print -quit 2>/dev/null)" ]; then
	echo "error: dataset '$DS' not found (or empty) at $DIR" >&2
	echo "       populate it with aisio's tasks/setup_dataset.yaml on the target" >&2
	exit 1
fi

echo "dataset '$DS' ready: $DIR ($(du -sh "$DIR" 2>/dev/null | cut -f1))"
