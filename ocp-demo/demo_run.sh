#!/bin/bash
# Multi-drive opends load demo runner (smrc). Usage:
#   demo_run.sh <first-shard-path> <load-mode> [extra llama-cli args...]

SHARD1="$1"; shift
MODE="$1"; shift
LOG=/root/demo_${MODE}.log

export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu
export PATH=/usr/local/cuda/bin:$PATH

# two workers per drive; the aisio default is one per device in a multi-device group
export OPENDS_AISIO_IO_THREADS=${OPENDS_AISIO_IO_THREADS:-8}
export OPENDS_AISIO_QUEUE_DEPTH=${OPENDS_AISIO_QUEUE_DEPTH:-32}

# refuse to run against the stale opends deps in nikhil's tree
BAD=$(ldd /root/llama.cpp/build/bin/llama-cli | grep -E "libxnvme|libxal|libopends" | grep -v /usr/local || true)
if [ -n "$BAD" ]; then
    echo "wrong runtime libs:"; echo "$BAD"; exit 3
fi

sync
echo 3 > /proc/sys/vm/drop_caches

BEFORE=$(awk '$3 ~ /^ublkb[0-3]$/ {print $3, $6}' /proc/diskstats)

prlimit --nofile=1048576:1048576 \
/root/llama.cpp/build/bin/llama-cli -m "$SHARD1" -ngl 99 \
  -ot "token_embd.weight=CUDA0" --load-mode "$MODE" \
  -p "hello" -n 16 -st --no-warmup -v "$@" < /dev/null > "$LOG" 2>&1
RC=$?

AFTER=$(awk '$3 ~ /^ublkb[0-3]$/ {print $3, $6}' /proc/diskstats)

echo "rc=$RC log=$LOG"
grep -E "opends|load_all_data" "$LOG" | tail -n 12
echo "--- generated:"
grep -A2 "^hello" "$LOG" | head -5
echo "--- sectors read (delta MiB):"
join <(echo "$BEFORE") <(echo "$AFTER") | awk '{printf "%s %d -> %d  delta %.0f MiB\n", $1, $2, $3, ($3-$2)/2048}'
