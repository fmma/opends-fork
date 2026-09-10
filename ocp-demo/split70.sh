#!/bin/bash
# Merge the 70B Q6_K parts, then split by bytes: 4 ways (one shard per mount) and 2 ways.
# --split-max-size packs whole tensors, so the limit needs a few percent of slack over
# total/N or an extra shard spills out; --dry-run shows the plan.
set -e
SRC=/mnt/ssd0/models_src/llama33-70b/Llama-3.3-70B-Instruct-Q6_K
MERGED=/mnt/ssd0/models_src/Llama-3.3-70B-Instruct-Q6_K.gguf
P4=/mnt/ssd0/models/llama33-70b-q6k
P2=/mnt/ssd0/models/llama33-70b-2way
BIN=/root/llama.cpp/build/bin/llama-gguf-split
export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu

if [ ! -s "$MERGED" ]; then
    $BIN --merge $SRC/Llama-3.3-70B-Instruct-Q6_K-00001-of-00002.gguf "$MERGED"
fi

# 4 ways: 14600M gives 14424-14548 MB shards; 14500M spills a fifth shard
$BIN --split --split-max-size 14600M "$MERGED" "$P4"
for i in 2 3 4; do
    mnt=/mnt/ssd$((i-1))
    mkdir -p $mnt/models
    mv ${P4}-0000$i-of-00004.gguf $mnt/models/
    ln -sf $mnt/models/llama33-70b-q6k-0000$i-of-00004.gguf /mnt/ssd0/models/
done

# 2 ways: 29000M gives 28915M + 28972M; 27900M spills a third shard
$BIN --split --split-max-size 29000M "$MERGED" "$P2"
mv ${P2}-00002-of-00002.gguf /mnt/ssd1/models/
ln -sf /mnt/ssd1/models/llama33-70b-2way-00002-of-00002.gguf /mnt/ssd0/models/

sync
ls -lL /mnt/ssd0/models/llama33-70b-*
echo "now restart xal-server"
