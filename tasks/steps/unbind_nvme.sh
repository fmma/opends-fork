#!/bin/bash
# Unmount and unbind an NVMe device from the kernel nvme driver.
# Skips if the guard file is missing, which signals that the aisio
# backend was not built and nothing needs the device detached.
set -e

GUARD=$1
BDF=$2
MOUNT=$3

if [ ! -e "$GUARD" ]; then
	echo "skipping: $GUARD not present (aisio backend not built)"
	exit 0
fi

echo "unmounting $MOUNT"
umount "$MOUNT"

echo "unbinding $BDF from nvme"
echo "$BDF" > /sys/bus/pci/drivers/nvme/unbind

if [ -e "/sys/bus/pci/devices/$BDF/driver" ]; then
	DRIVER=$(basename "$(readlink "/sys/bus/pci/devices/$BDF/driver")")
	echo "error: $BDF still bound to $DRIVER after unbind"
	exit 1
fi
echo "$BDF unbound"
