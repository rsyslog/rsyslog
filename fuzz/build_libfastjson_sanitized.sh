#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
#
# This file is part of rsyslog.
# Released under ASL 2.0

## build_libfastjson_sanitized.sh
## Build an external libfastjson checkout with the same sanitizer/fuzz flags.
##
## This repository does not vendor libfastjson. For full sanitizer coverage of
## JSON paths, build libfastjson separately, install it to a temporary prefix,
## and configure rsyslog with PKG_CONFIG_PATH pointing at that prefix.

set -eu

usage() {
	cat >&2 <<'USAGE'
usage: fuzz/build_libfastjson_sanitized.sh LIBFASTJSON_SRC [PREFIX]

Environment:
  CC                 compiler, defaults to clang
  FUZZ_SANITIZERS    sanitizer list, defaults to address,undefined
  FUZZ_COVERAGE      set to 1/yes/true to add --coverage
  FUZZ_CFLAGS        extra compile flags
  FUZZ_LDFLAGS       extra link flags
  JOBS               make parallelism
USAGE
	exit 2
}

[ "$#" -ge 1 ] || usage
[ "$#" -le 2 ] || usage

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
src=$1
prefix=${2:-"$repo_dir/fuzz/libfastjson-instrumented"}

case "$src" in
	/*) ;;
	*) src=$(pwd)/"$src" ;;
esac
case "$prefix" in
	/*) ;;
	*) prefix=$(pwd)/"$prefix" ;;
esac

[ -d "$src" ] || {
	printf 'libfastjson source directory does not exist: %s\n' "$src" >&2
	exit 1
}

cc=${CC:-clang}
sanitizers=${FUZZ_SANITIZERS:-address,undefined}
coverage=${FUZZ_COVERAGE:-0}
cflags="${CFLAGS:-} ${FUZZ_CFLAGS:-} -g -O1 -fno-omit-frame-pointer"
ldflags="${LDFLAGS:-} ${FUZZ_LDFLAGS:-}"

if [ -n "$sanitizers" ]; then
	cflags="$cflags -fsanitize=$sanitizers"
	ldflags="$ldflags -fsanitize=$sanitizers"
fi
case "$coverage" in
	1|yes|true)
		cflags="$cflags --coverage"
		ldflags="$ldflags --coverage"
		;;
esac

jobs=${JOBS:-}
if [ -z "$jobs" ]; then
	jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')
fi

(
	cd "$src"
	if [ ! -x ./configure ]; then
		if [ -x ./autogen.sh ]; then
			./autogen.sh
		else
			autoreconf -fvi
		fi
	fi
	if [ -f Makefile ]; then
		make clean
	fi
	CC="$cc" CFLAGS="$cflags" LDFLAGS="$ldflags" ./configure \
		--prefix="$prefix" --disable-shared --enable-static ${LIBFASTJSON_CONFIGURE_FLAGS:-}
	make -j"$jobs"
	make install
)

cat <<EOF
Instrumented libfastjson installed to:
  $prefix

Use it when configuring rsyslog:
  export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:\${PKG_CONFIG_PATH:-}"
  export CC="$cc"
  export CFLAGS="$cflags"
  export LDFLAGS="$ldflags"
  ./configure --enable-testbench --enable-imdiag --enable-omstdout
EOF
