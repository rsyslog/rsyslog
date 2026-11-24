#!/bin/bash -eu
# SPDX-License-Identifier: Apache-2.0
#
# Build rsyslog's libFuzzer targets for OSS-Fuzz.
#
# google/oss-fuzz projects/rsyslog/build.sh only calls this script, so the
# build recipe, the target list, and the seed corpora are maintained here
# together with the code. The OSS-Fuzz Dockerfile clones the dependencies
# below into $SRC; everything else comes from the OSS-Fuzz build environment
# ($SRC, $WORK, $OUT, $CC, $CFLAGS, $LIB_FUZZING_ENGINE).
#
# The runner image only provides $OUT, so dependencies are built from source,
# instrumented, and linked statically. rsyslog's runtime modules (lm*.so)
# remain loadable modules: they are copied to $OUT/rsyslog-modules, where the
# targets find them (tools/fuzz_moddir.h).
#
# Because the dependencies are static, modules that use zlib or zstd (for
# example lmtcpsrv.so) carry their own copy. Both libraries keep no mutable
# global state, so this is harmless; ASan reports it as an ODR violation,
# which the OSS-Fuzz runners disable (detect_odr_violation=0). Set the same
# option when running these binaries outside OSS-Fuzz.
#
# Copyright 2026 Rainer Gerhards and Others
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#       -or-
#       see COPYING.ASL20 in the source distribution
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Fuzz targets built from tools/ and shipped to $OUT. Add new targets here.
FUZZ_TARGETS="fuzz_rsyslog_message fuzz_imtcp_session"

DEPS="$WORK/deps"
JOBS="$(nproc)"
export PKG_CONFIG_PATH="$DEPS/lib/pkgconfig"
# rsyslog's module ABI calls through generic function pointers, as in the
# project's own fuzz CI. -fPIC: the static dependencies are also linked into
# the dlopen()ed runtime modules.
export CFLAGS="$CFLAGS -fno-sanitize=function -fPIC"
export CXXFLAGS="$CXXFLAGS -fno-sanitize=function -fPIC"

topdir="$(cd "$(dirname "$0")/../.." && pwd)"

# --- static, instrumented dependencies -------------------------------------
(cd "$SRC/zlib" && ./configure --static --prefix="$DEPS" && make -j"$JOBS" install)

(cd "$SRC/zstd" &&
	make -C lib -j"$JOBS" libzstd.a libzstd.pc PREFIX="$DEPS" &&
	make -C lib install-static install-includes install-pc PREFIX="$DEPS")

for lib in libestr libfastjson; do
	(cd "$SRC/$lib" &&
		autoreconf -fi &&
		./configure --prefix="$DEPS" --disable-shared --enable-static &&
		make -j"$JOBS" install)
done

# --- rsyslog -----------------------------------------------------------------
cd "$topdir"
autoreconf -fi
# zstd installs libzstd.pc, so configure uses its -lzstd fallback; point it at
# $DEPS. LIBS repeats the static archives at the end of every link line
# because libtool places librsyslog.a after $(ZLIB_LIBS) for the fuzz targets.
CPPFLAGS="-I$DEPS/include" LDFLAGS="-L$DEPS/lib" LIBS="-lzstd -lz -lpthread" \
	./configure \
	--enable-fuzzing LIB_FUZZING_ENGINE="$LIB_FUZZING_ENGINE" \
	--enable-libzstd --disable-libgcrypt --disable-uuid --disable-libsystemd \
	--disable-libyaml --disable-fmhttp --disable-klog --disable-default-tests \
	--disable-impstats-push --disable-generate-man-pages
make -j"$JOBS"
# shellcheck disable=SC2086 # FUZZ_TARGETS is a word list
make -C tools -j"$JOBS" $FUZZ_TARGETS

mkdir -p "$OUT/rsyslog-modules"
cp runtime/.libs/lm*.so "$OUT/rsyslog-modules/"
for target in $FUZZ_TARGETS; do
	# libtool may leave a wrapper script in tools/ and the binary in .libs/.
	if [ -x "tools/.libs/$target" ]; then
		cp "tools/.libs/$target" "$OUT/"
	else
		cp "tools/$target" "$OUT/"
	fi
	# fuzz_imtcp_session links lmtcpsrv.so directly; resolve it next to the
	# binary. Harmless for targets without such a dependency.
	# shellcheck disable=SC2016 # $ORIGIN is for the dynamic loader
	patchelf --set-rpath '$ORIGIN/rsyslog-modules' "$OUT/$target"
done

# --- seed corpora --------------------------------------------------------------
zip -j -q "$OUT/fuzz_rsyslog_message_seed_corpus.zip" tests/fuzz/corpus/syslog-message/*
python3 tests/fuzz/prepare-imtcp-session-corpus.py \
	tests/fuzz/corpus/imtcp-session "$WORK/imtcp-session-corpus"
zip -j -q "$OUT/fuzz_imtcp_session_seed_corpus.zip" "$WORK"/imtcp-session-corpus/*
