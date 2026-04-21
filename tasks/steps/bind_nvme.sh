#!/bin/bash
# Bind NVMe device to kernel driver.
set -e

if [ $# -ne 2 ]; then
	echo "usage: bind_nvme.sh BDF NS" >&2
	exit 1
fi

BDF=$1
NS=$2

if [ ! -d "/sys/bus/pci/devices/$BDF" ]; then
	echo "error: PCI device $BDF not found in sysfs"
	ls /sys/bus/pci/devices/
	exit 1
fi

DRIVER=$(basename "$(readlink "/sys/bus/pci/devices/$BDF/driver")" 2>/dev/null)
if [ "$DRIVER" != "nvme" ]; then
	if [ -n "$DRIVER" ]; then
		echo "unbinding $BDF from $DRIVER"
		echo "$BDF" > "/sys/bus/pci/drivers/$DRIVER/unbind"
	fi

	echo "resetting $BDF"
	echo 1 > "/sys/bus/pci/devices/$BDF/reset"

	echo "binding $BDF to nvme"
	echo "$BDF" > /sys/bus/pci/drivers/nvme/bind
	udevadm settle
fi

if [ ! -b "$NS" ]; then
	echo "error: $NS did not appear after binding $BDF to nvme"
	echo "available block devices:"
	ls /dev/nvme* || true
	exit 1
fi

echo "$NS is ready"
