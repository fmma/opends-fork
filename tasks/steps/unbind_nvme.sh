#!/bin/bash
# Unmount and unbind an NVMe device from the kernel nvme driver.
set -e

BDF=$1
MOUNT=$2

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
