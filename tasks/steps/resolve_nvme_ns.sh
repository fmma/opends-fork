#!/bin/bash
# Resolve /dev/nvmeXnY for the namespace currently bound under BDF.
#
# Kernel renumbers the device name on each bind whenever earlier
# numbers cannot be reused (e.g. nvidia_fs leaving an nvme module
# refcount across runs). Walk /sys/block/nvme* and pick the entry
# whose backing device sits under the given BDF.
set -e

if [ $# -ne 1 ]; then
	echo "usage: resolve_nvme_ns.sh BDF" >&2
	exit 1
fi

BDF=$1

for blk in /sys/block/nvme*; do
	[ -e "$blk/device" ] || continue
	real=$(readlink -f "$blk/device")
	case "$real" in
		*/${BDF}/*) echo "/dev/$(basename "$blk")"; exit 0 ;;
	esac
done

echo "error: no /dev/nvme*n* under $BDF" >&2
exit 1
