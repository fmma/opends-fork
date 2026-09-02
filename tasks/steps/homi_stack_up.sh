#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Bring up the upstream HOMI/qublk/xal-server stack so the aisio backend can
# join it and read files on a mounted filesystem. Hands the NVMe controller to
# userspace (homi start), exposes it as a ublk block device (qublk), mounts
# the existing XFS over it at MOUNT, and publishes the mount's extent index
# over shared memory (xal-server). The same XFS is what the ref/gds phases
# mounted via the kernel driver, so any file written there (e.g. the sync-read
# pattern) is still present after the remount.
set -e

if [ $# -ne 2 ]; then
	echo "usage: homi_stack_up.sh BDF MOUNT" >&2
	exit 1
fi

BDF=$1
MOUNT=$2
HERE=$(dirname "$0")
HOMI_ID=1
XAL_SHM=/xal_dev0
CONF=/run/homi/xal-server.conf

mkdir -p /run/homi

# Clear any stale stack from a crashed or aborted prior run before bringing up
# a fresh one.
"$HERE/stop_homi_stack.sh"

# Hand the controller to userspace (unmounts the kernel mount, unbinds nvme,
# leaves the controller in a clean power-on state).
"$HERE/unbind_nvme.sh" "$BDF" "$MOUNT"

# Diagnostic: let the daemons dump a core on crash (paired with a file
# core_pattern). Harmless when cores are disabled by policy.
ulimit -c unlimited 2>/dev/null || true

# The server's host heap is the pool every secondary draws from; the aisio
# driver alone asks for 256 MiB by default.
echo "starting homi"
setsid homi start "$BDF" --homi-id "$HOMI_ID" --be upcie \
	--host_heap_size $((512 * 1024 * 1024)) \
	< /dev/null > /run/homi/homi.log 2>&1 &
# 'homi status' exits non-zero until the server is up and its devices ready.
for _ in $(seq 1 120); do
	homi status --homi-id "$HOMI_ID" > /dev/null 2>&1 && break
	if ! pgrep -x homi > /dev/null; then
		echo "error: homi exited during startup" >&2
		cat /run/homi/homi.log >&2 || true
		exit 1
	fi
	sleep 0.5
done
if ! homi status --homi-id "$HOMI_ID" > /dev/null 2>&1; then
	echo "error: homi did not become ready" >&2
	cat /run/homi/homi.log >&2 || true
	exit 1
fi

echo "starting qublk"
setsid qublk run "$BDF" --be upcie --homi-id "$HOMI_ID" --nqueues 1 \
	< /dev/null > /run/homi/qublk.log 2>&1 &
UBLK=""
for _ in $(seq 1 60); do
	UBLK=$(grep -oE '/dev/ublkb[0-9]+' /run/homi/qublk.log 2>/dev/null | head -1)
	[ -n "$UBLK" ] && [ -b "$UBLK" ] && break
	sleep 0.3
done
if [ -z "$UBLK" ] || [ ! -b "$UBLK" ]; then
	echo "error: qublk did not expose a ublk device" >&2
	cat /run/homi/qublk.log >&2 || true
	exit 1
fi
echo "$UBLK" > /run/homi/ublk_dev

mkdir -p "$MOUNT"
mount "$UBLK" "$MOUNT"

# watchmode 2 = extent update: the server re-indexes on filesystem changes,
# which is what makes the extents of freshly written files resolvable.
cat > "$CONF" <<EOF
log_level = 2
devices = [
  { uri = "$UBLK", shm_name = "$XAL_SHM", mountpoint = "$MOUNT" },
]

[xal]
watchmode = 2
EOF

echo "starting xal-server"
setsid xal-server --config "$CONF" < /dev/null > /run/homi/xal-server.log 2>&1 &
# The shm region appearing is enough; clients retry -EAGAIN/-ESTALE until the
# first index completes.
for _ in $(seq 1 120); do
	[ -e "/dev/shm${XAL_SHM}_state" ] && break
	if ! pgrep -x xal-server > /dev/null; then
		echo "error: xal-server exited during startup" >&2
		cat /run/homi/xal-server.log >&2 || true
		journalctl -t xal-server -n 20 --no-pager >&2 || true
		exit 1
	fi
	sleep 0.5
done
if [ ! -e "/dev/shm${XAL_SHM}_state" ]; then
	echo "error: xal-server did not publish $XAL_SHM" >&2
	cat /run/homi/xal-server.log >&2 || true
	journalctl -t xal-server -n 20 --no-pager >&2 || true
	exit 1
fi

echo "HOMI stack up: homi + qublk ($UBLK) + xal-server mounted at $MOUNT"
