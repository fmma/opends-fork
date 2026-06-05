#!/bin/bash
# Bring up the HOMI/qublk stack so the aisio backend can attach qpairs and read
# files on a mounted filesystem. Hands the NVMe controller to userspace (HOMI),
# exposes it as a ublk block device (qublk), and mounts the existing XFS over it
# at MOUNT. The same XFS is what the ref/gds phases mounted via the kernel
# driver, so any file written there (e.g. the sync-read pattern) is still
# present after the remount.
set -e

if [ $# -ne 2 ]; then
	echo "usage: homi_stack_up.sh BDF MOUNT" >&2
	exit 1
fi

BDF=$1
MOUNT=$2
HERE=$(dirname "$0")
SOCK=/run/homi/homi.sock
CONF=/run/homi/homi-test.conf

mkdir -p /run/homi

# Clean any stale stack from a crashed prior run: stop the servers, drop a
# lingering ublk mount, and reload ublk_drv to clear stale device ids.
if findmnt -no SOURCE "$MOUNT" 2>/dev/null | grep -q ublkb; then
	umount "$MOUNT" 2>/dev/null || umount -l "$MOUNT" 2>/dev/null || true
fi
pkill -KILL -x qublk 2>/dev/null || true
pkill -KILL -x homid 2>/dev/null || true
sleep 1
rmmod ublk_drv 2>/dev/null || true
modprobe ublk_drv

# Hand the controller to userspace (unmounts the kernel mount, unbinds nvme,
# leaves the controller in a clean power-on state).
"$HERE/unbind_nvme.sh" "$BDF" "$MOUNT"

cat > "$CONF" <<EOF
log_level = 2
devices = [ "$BDF" ]
ipc_socket = "$SOCK"

[xal]
# XFS (1), not FIEMAP (2): homid owns the controller raw over upcie with the
# kernel nvme driver unbound, so xal parses on-disk XFS metadata over the
# xnvme device rather than FIEMAP-ing a kernel mount.
backend = 1
watchmode = 0
file_lookupmode = 0
EOF
rm -f "$SOCK" /run/homi/*.desc

echo "starting homid"
# Detach the daemon fully: redirect on the outer command so neither the daemon
# nor any wrapper keeps this step's stdout/stderr open. Otherwise the SSH
# channel never sees EOF and the cijoe step (and plain ssh) hangs even though
# the daemon started fine.
setsid homid --config "$CONF" < /dev/null > /run/homi/homid.log 2>&1 &
# homid is launched via a bash wrapper, so the named process appears a moment
# after the '&'. Wait for it to show up before watching for it to die.
for _ in $(seq 1 20); do
	pgrep -x homid > /dev/null && break
	sleep 0.2
done
# Wait for the listening socket, bailing if homid dies. homid binds the socket
# only after owning the controller and indexing xal (a few seconds), so the
# socket's appearance means it is ready to serve qpair-attach and xal requests.
for _ in $(seq 1 120); do
	[ -S "$SOCK" ] && break
	if ! pgrep -x homid > /dev/null; then
		echo "error: homid exited during startup (see: journalctl -t homi)" >&2
		exit 1
	fi
	sleep 0.5
done
if [ ! -S "$SOCK" ]; then
	echo "error: homid socket did not come up" >&2
	exit 1
fi

echo "starting qublk"
setsid qublk --homi "$SOCK" --dev-uri "$BDF" --nr-queues 1 < /dev/null > /run/homi/qublk.log 2>&1 &
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
echo "HOMI stack up: homid + qublk ($UBLK) mounted at $MOUNT"
