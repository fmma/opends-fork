#!/bin/bash
# Stage 1: homi holds the four drives, one qublk per drive -> /dev/ublkb0..3.
set -u
export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu
export PATH=/usr/local/cuda/bin:$PATH

DRIVES=(0000:4a:00.0 0000:4b:00.0 0000:4c:00.0 0000:4d:00.0)
HOMI_ID=1
LOG=/root/ocp/log
mkdir -p "$LOG"

# homi group order is build-dependent (safl homi_gpu = arg order, old = reverse); check 'homi status'.
setsid /usr/local/bin/homi start "${DRIVES[@]}" --homi-id "$HOMI_ID" \
    --be upcie --host_heap_size 4294967296 \
    > "$LOG/homi.log" 2>&1 < /dev/null &
echo "homi pid=$!"
sleep 4
if grep -qiE "error|fail|abort" "$LOG/homi.log"; then
    echo "== homi log (suspect) =="; tail -15 "$LOG/homi.log"
fi

for i in 0 1 2 3; do
    setsid /usr/local/bin/qublk run "${DRIVES[$i]}" --be upcie \
        --homi-id "$HOMI_ID" --nqueues 1 --qdepth 8 --dev-id "$i" \
        > "$LOG/qublk$i.log" 2>&1 < /dev/null &
    echo "qublk$i (${DRIVES[$i]}) pid=$!"
    for t in $(seq 1 30); do [ -b /dev/ublkb$i ] && break; sleep 1; done
    if [ -b /dev/ublkb$i ]; then
        echo "  ublkb$i up"
    else
        echo "  ublkb$i MISSING"; tail -8 "$LOG/qublk$i.log"
    fi
done

echo "== ublk block devices =="
lsblk -o NAME,SIZE,TYPE 2>/dev/null | grep -E "ublkb|NAME"
echo "== homi status =="
LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu /usr/local/bin/homi status 2>&1 | head -20
