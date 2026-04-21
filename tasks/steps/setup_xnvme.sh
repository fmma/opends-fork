#!/bin/bash
set -e

if [ $# -ne 4 ]; then
	echo "usage: setup_xnvme.sh REMOTE BRANCH SRC_DIR BUILD_DIR" >&2
	exit 1
fi

REMOTE=$1
BRANCH=$2
SRC_DIR=$3
BUILD_DIR=$4

if [ ! -d "$SRC_DIR/.git" ]; then
	rm -rf "$SRC_DIR"
	git clone --recursive "$REMOTE" "$SRC_DIR"
fi

cd "$SRC_DIR"
git fetch --all
git checkout "$BRANCH"
git pull --rebase
git submodule update --init --recursive

rm -rf "$BUILD_DIR"
meson setup "$BUILD_DIR" "$SRC_DIR"
meson compile -C "$BUILD_DIR"
meson install -C "$BUILD_DIR"
ldconfig
