#!/bin/bash
# Rebind an NVMe device to the kernel nvme driver.
# Skips if the guard file is missing, which signals that the aisio
# backend was not built and the device was never unbound.
set -e

if [ $# -ne 2 ]; then
	echo "usage: rebind_nvme.sh GUARD BDF" >&2
	exit 1
fi

GUARD=$1
BDF=$2

if [ ! -e "$GUARD" ]; then
	echo "skipping: $GUARD not present (aisio backend not built)"
	exit 0
fi

echo "rebinding $BDF to nvme"
echo "$BDF" > /sys/bus/pci/drivers/nvme/bind

if [ ! -e "/sys/bus/pci/devices/$BDF/driver" ]; then
	echo "error: $BDF has no driver after bind"
	exit 1
fi
DRIVER=$(basename "$(readlink "/sys/bus/pci/devices/$BDF/driver")")
if [ "$DRIVER" != "nvme" ]; then
	echo "error: $BDF bound to $DRIVER, expected nvme"
	exit 1
fi
echo "$BDF bound to nvme"
