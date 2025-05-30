#!/bin/bash
# script for generic CI runs via container
printf 'running CI with\n'
printf 'container: %s\n' $RSYSLOG_DEV_CONTAINER
printf 'CC:\t%s\n' "$CC"
printf 'CFLAGS:\t%s:\n' "$CFLAGS"
printf 'RSYSLOG_CONFIGURE_OPTIONS:\t%s\n' "$RSYSLOG_CONFIGURE_OPTIONS"
printf 'working directory: %s\n' "$(pwd)"
printf 'user ids: %s:%s\n' $(id -u) $(id -g)
if [ "$SUDO" != "" ]; then
	printf 'check sudo'
	$SUDO echo sudo works!
fi
if [ "$CI_VALGRIND_SUPPRESSIONS" != "" ]; then
	export RS_TESTBENCH_VALGRIND_EXTRA_OPTS="--suppressions=$(pwd)/tests/CI/$CI_VALGRIND_SUPPRESSIONS"
fi
if [ "$CI_SANITIZE_BLACKLIST" != "" ]; then
	export CFLAGS="$CFLAGS -fsanitize-blacklist=$(pwd)/$CI_SANITIZE_BLACKLIST"
	printf 'CFLAGS changed to: %s\n', "$CFLAGS"
fi
set -e

printf 'STEP: autoreconf / configure ===============================================\n'
devtools/run-configure.sh

if [ "$CI_CHECK_CMD" != "distcheck" ]; then
	printf 'STEP: make =================================================================\n'
	make $CI_MAKE_OPT
fi

printf 'STEP: make %s ==============================================================\n', \
	"$CI_CHECK_CMD"
set +e
echo CI_CHECK_CMD: $CI_CHECK_CMD
make $CI_MAKE_CHECK_OPT ${CI_CHECK_CMD:-check}
rc=$?

printf 'STEP: find failing tests ====================================================\n'
echo calling gather-check-logs
devtools/gather-check-logs.sh

printf 'STEP: Codecov upload =======================================================\n'
if [ "$CI_CODECOV_TOKEN" != "" ]; then
	curl -s https://codecov.io/bash >codecov.sh
	chmod +x codecov.sh
	./codecov.sh -t "$CI_CODECOV_TOKEN" -n 'rsyslog buildbot PR' &> codecov_log
	rm codecov.sh
	lines="$(wc -l < codecov_log)"
	if (( lines > 3000 )); then
		printf 'codecov log file is very large (%d lines), showing parts\n' $lines
		head -n 1500 < codecov_log
		printf '\n\n... snip ...\n\n'
		tail -n 1500 < codecov_log
	else
		cat codecov_log
	fi
	rm codecov_log
fi

exit $rc
