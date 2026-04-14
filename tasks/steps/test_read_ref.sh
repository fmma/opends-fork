#!/bin/bash
set -e

BUILD_DIR=$1
TEST_FILE=$2

if [ ! -x "$BUILD_DIR/test_read_ref" ]; then
	echo "error: $BUILD_DIR/test_read_ref not found"
	ls "$BUILD_DIR"/ || true
	exit 1
fi

if [ ! -f "$TEST_FILE" ]; then
	echo "error: test file $TEST_FILE does not exist"
	exit 1
fi

echo 3 > /proc/sys/vm/drop_caches
mkdir -p "$BUILD_DIR/read_output"
"$BUILD_DIR/test_read_ref" "$TEST_FILE" "$BUILD_DIR/read_output/ref.out"
