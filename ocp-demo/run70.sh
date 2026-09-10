#!/bin/bash
M=/mnt/ssd0/models/llama33-70b-q6k-00001-of-00004.gguf
echo "=== opends defaults"
/root/demo_run.sh $M opends -c 4096
cp /root/demo_opends.log /root/demo70_opends_def.log
echo "=== opends 8 busy"
OPENDS_AISIO_IO_THREADS=8 OPENDS_AISIO_IDLE_SPIN=busy /root/demo_run.sh $M opends -c 4096
cp /root/demo_opends.log /root/demo70_opends_t8.log
echo "=== dio"
/root/demo_run.sh $M dio -c 4096
echo "=== mmap"
/root/demo_run.sh $M mmap -c 4096
echo "=== all done"
