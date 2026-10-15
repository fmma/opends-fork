#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Source with the install prefix as the argument. Sets MESON_ARGS for meson
# setup and the environment to build against what is already in PREFIX. A
# prefix other than /usr/local gets a flat lib dir, an rpath so its binaries
# find their libraries without ldconfig, and a systemd.pc that keeps homi's
# unit inside the prefix (pkgconf ignores PKG_CONFIG_*_* overrides).
PREFIX=${1:-/usr/local}
MESON_ARGS=()
if [ "$PREFIX" != /usr/local ]; then
	MESON_ARGS=(--prefix "$PREFIX" --libdir lib)
	export PATH="$PREFIX/bin:$PATH"
	export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
	export LDFLAGS="-Wl,-rpath,$PREFIX/lib${LDFLAGS:+ $LDFLAGS}"
	if [ ! -f "$PREFIX/lib/pkgconfig/systemd.pc" ]; then
		mkdir -p "$PREFIX/lib/pkgconfig"
		cat > "$PREFIX/lib/pkgconfig/systemd.pc" <<PC
prefix=$PREFIX
systemdsystemunitdir=\${prefix}/lib/systemd/system
systemduserunitdir=\${prefix}/lib/systemd/user

Name: systemd
Description: unit directories under $PREFIX
Version: $(pkg-config --modversion systemd 2>/dev/null || echo 0)
PC
	fi
fi
