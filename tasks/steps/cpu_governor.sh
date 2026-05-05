#!/bin/bash
# Put the CPU in a stable bench mode: schedutil governor + boost disabled.
# Reverts to the kernel default on reboot.
set -e

GOV=schedutil

for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
	echo "$GOV" > "$g"
done

if [ -w /sys/devices/system/cpu/cpufreq/boost ]; then
	echo 0 > /sys/devices/system/cpu/cpufreq/boost
fi

echo "governor=$GOV, boost=$(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null || echo n/a)"
