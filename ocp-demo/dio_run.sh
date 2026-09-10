#!/bin/bash
# dio load runner (smrc). Reads via kernel nvme, samples the four nvme
# namespaces backing the demo drives. Usage: dio_run.sh <first-shard> <mode> [args...]
SHARD1="$1"; shift
MODE="$1"; shift
LOG=/root/dio_${MODE}.log
NS="nvme4n1 nvme7n1 nvme12n1 nvme14n1"  # all-fast; namespaces shift after reboot/rebind

export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu
export PATH=/usr/local/cuda/bin:$PATH

sync
echo 3 > /proc/sys/vm/drop_caches

BEFORE=$(awk -v ns="$NS" 'BEGIN{n=split(ns,a," ");for(i=1;i<=n;i++)want[a[i]]=1} $3 in want {print $3, $6}' /proc/diskstats)
T0=$(date +%s.%N)

prlimit --nofile=1048576:1048576 \
/root/llama.cpp/build/bin/llama-cli -m "$SHARD1" -ngl 99 \
  -ot "token_embd.weight=CUDA0" --load-mode "$MODE" \
  -p "hello" -n 16 -st --no-warmup -v "$@" < /dev/null > "$LOG" 2>&1
RC=$?

T1=$(date +%s.%N)
AFTER=$(awk -v ns="$NS" 'BEGIN{n=split(ns,a," ");for(i=1;i<=n;i++)want[a[i]]=1} $3 in want {print $3, $6}' /proc/diskstats)

echo "rc=$RC log=$LOG wall=$(echo "$T1-$T0"|bc)s"
grep -iE "load_all_data|load time|loading model|model load" "$LOG" | tail -n 12
echo "--- generated:"
grep -A2 "^hello" "$LOG" | head -5
echo "--- sectors read (delta MiB):"
join <(echo "$BEFORE"|sort) <(echo "$AFTER"|sort) | awk '{printf "%s %d -> %d  delta %.0f MiB\n", $1, $2, $3, ($3-$2)/2048}'
