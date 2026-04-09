#!/bin/bash
set -e

TEST_FILE=$1
DIR=$(dirname "$TEST_FILE")

if [ ! -d "$DIR" ]; then
	echo "error: directory $DIR does not exist"
	exit 1
fi

echo "generating $TEST_FILE"
dd if=/dev/urandom of="$TEST_FILE" bs=1M count=2 oflag=direct conv=fsync
