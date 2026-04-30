#!/bin/bash
set -e

if [ $# -ne 4 ]; then
	echo "usage: setup_meson_dep.sh REMOTE REF SRC_DIR BUILD_DIR" >&2
	echo "  REF is a branch, tag, or commit hash" >&2
	exit 1
fi

REMOTE=$1
REF=$2
SRC_DIR=$3
BUILD_DIR=$4

if [ ! -d "$SRC_DIR/.git" ]; then
	rm -rf "$SRC_DIR"
	git clone --recursive "$REMOTE" "$SRC_DIR"
fi

cd "$SRC_DIR"
git remote set-url origin "$REMOTE"
git fetch --all --tags
# Prefer origin/REF so branch refs pick up the latest remote tip; fall
# back to REF directly for tags and commit hashes.
if git rev-parse --verify --quiet "origin/$REF^{commit}" > /dev/null; then
	git checkout --detach "origin/$REF"
else
	git checkout --detach "$REF"
fi
git submodule update --init --recursive

rm -rf "$BUILD_DIR"
meson setup "$BUILD_DIR" "$SRC_DIR"
meson compile -C "$BUILD_DIR"
meson install -C "$BUILD_DIR"
ldconfig
