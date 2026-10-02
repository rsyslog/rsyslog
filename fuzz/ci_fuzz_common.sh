## ci_fuzz_common.sh
## Shared GitLab CI helpers for building rsyslog AFL++ fuzz targets.

ci_make_jobs() {
	if [ -n "${MAKE_JOBS:-}" ]; then
		printf '%s\n' "$MAKE_JOBS"
		return
	fi

	nproc
}

ci_build_afl_target() {
	unset AFL_USE_UBSAN
	export AFL_USE_ASAN=${AFL_USE_ASAN:-1}
	export CC=${CC:-afl-clang-fast}
	export CFLAGS=${CFLAGS:--g -O1 -fno-omit-frame-pointer}
	default_configure_flags="--enable-testbench --enable-imdiag --enable-omstdout"
	default_configure_flags="$default_configure_flags --disable-libgcrypt --disable-fmhttp"
	default_configure_flags="$default_configure_flags --disable-generate-man-pages"
	default_configure_flags="$default_configure_flags --disable-impstats --disable-impstats-push"
	default_configure_flags="$default_configure_flags --disable-libyaml"
	default_configure_flags="$default_configure_flags --disable-Werror"

	NOCONFIGURE=1 ./autogen.sh
	# shellcheck disable=SC2086
	./configure ${RSYSLOG_FUZZ_CONFIGURE_FLAGS:-$default_configure_flags}
	make -j"$(ci_make_jobs)" fuzz \
		FUZZ_COVERAGE=0 \
		FUZZ_SANITIZERS= \
		FUZZ_NO_SANITIZE_FLAGS=-fno-sanitize=function
}

ci_set_afl_preload() {
	if [ -n "${AFL_PRELOAD:-}" ] || [ "${RSYSLOG_FUZZ_AUTO_AFL_PRELOAD:-0}" != "1" ]; then
		return
	fi

	preload=$(find "$PWD/runtime/.libs" -maxdepth 1 -name 'lm*.so' -print | sort | paste -sd: -)
	if [ -n "$preload" ]; then
		export AFL_PRELOAD="$preload"
	fi
}
