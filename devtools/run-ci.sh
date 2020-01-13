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
if [ "$TSAN_OPTIONS" != "" ]; then ## TODO ## IMPROVE CHECK
	export CFLAGS="$CFLAGS -fsanitize-blacklist=$(pwd)/tests/tsan.supp"
fi
set -e

printf 'STEP: autoreconf / configure ===============================================\n'
devtools/run-configure.sh

printf 'STEP: make =================================================================\n'
make $CI_MAKE_OPT

printf 'STEP: make %s ==============================================================\n', \
	"$CI_CHECK_CMD"
set +e
echo CI_CHECK_CMD: $CI_CHECK_CMD
make $CI_MAKE_CHECK_OPT ${CI_CHECK_CMD:-check}
rc=$?

# find failing tests
echo find failing tests
find . -name "*.trs" -exec bash -c 'if grep ":test-result: FAIL" "$1"; then printf "FAIL: ${1%%.trs} ################################################\\n" >> failed-tests.log; cat "${1%%trs}log"  >> failed-tests.log; fi' _ {} \;
if [ -f failed-tests.log ]; then
	# show summary stats so that we know how many failed
	head -n12 tests/test-suite.log >> failed.tests.log
fi

printf 'STEP: Codecov upload =======================================================\n'
if [ "$CI_CODECOV_TOKEN" != "" ]; then
	curl -s https://codecov.io/bash >codecov.sh
	chmod +x codecov.sh
	./codecov.sh -t "$CI_CODECOV_TOKEN" -n 'rsyslog buildbot PR'
	rm codecov.sh
fi

exit $rc
