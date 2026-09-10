#!/bin/bash
# Fresh XFS on each ublk block device, mounted ublkbN -> /mnt/ssdN.
set -u
for i in 0 1 2 3; do
    dev=/dev/ublkb$i
    mnt=/mnt/ssd$i
    if ! [ -b "$dev" ]; then echo "$dev MISSING, abort"; exit 1; fi
    umount "$mnt" 2>/dev/null
    echo "== mkfs.xfs $dev =="
    mkfs.xfs -f "$dev" >/dev/null 2>&1 && echo "  mkfs ok" || { echo "  mkfs FAIL"; exit 1; }
    mkdir -p "$mnt"
    mount "$dev" "$mnt" && echo "  mounted $mnt" || { echo "  mount FAIL"; exit 1; }
    mkdir -p "$mnt/models" "$mnt/models_src"
done
echo "== mounts =="
mount | grep /mnt/ssd
df -h /mnt/ssd0 /mnt/ssd1 /mnt/ssd2 /mnt/ssd3 | grep -E "ublk|Filesystem"
