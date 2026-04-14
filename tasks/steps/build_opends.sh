#!/bin/bash
set -e

SRC_DIR=$1
BUILD_DIR=$2

if [ ! -f "$SRC_DIR/meson.build" ]; then
	echo "error: $SRC_DIR/meson.build not found"
	exit 1
fi

rm -rf "$BUILD_DIR"
cd "$SRC_DIR"
meson setup "$BUILD_DIR"
meson compile -C "$BUILD_DIR"
