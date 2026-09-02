#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
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

# Untracked files survive the checkout (e.g. meson writes subprojects/
# into the source tree); only tracked modifications block.
if [ -n "$(git status --porcelain -uno)" ]; then
	echo "error: dependency checkout is not clean: $SRC_DIR" >&2
	git status --short >&2
	echo "       commit, stash, or 'git -C $SRC_DIR reset --hard' it, then retry" >&2
	exit 1
fi

git remote set-url origin "$REMOTE"
# --prune drops tracking refs of the old remote after a REMOTE change;
# they can collide with the new remote's branch names.
git fetch --all --tags --prune
# A pinned commit may sit on no branch (e.g. a PR head); fetch it by hash.
if ! git rev-parse --verify --quiet "origin/$REF^{commit}" > /dev/null &&
   ! git rev-parse --verify --quiet "$REF^{commit}" > /dev/null; then
	git fetch origin "$REF"
fi
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
