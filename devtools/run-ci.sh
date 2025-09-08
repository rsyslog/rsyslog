#!/bin/bash
## run-ci.sh
## Run generic CI tasks inside a container.
##
## This script is used by multiple CI systems and must remain
## independent of any specific CI provider.
printf 'running CI with\n'
printf 'container: %s\n' "$RSYSLOG_DEV_CONTAINER"
printf 'CC:\t%s\n' "$CC"
printf 'CFLAGS:\t%s:\n' "$CFLAGS"
printf 'RSYSLOG_CONFIGURE_OPTIONS:\t%s\n' "$RSYSLOG_CONFIGURE_OPTIONS"
printf 'working directory: %s\n' "$(pwd)"
printf 'user ids: %s:%s\n' "$(id -u)" "$(id -g)"
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
echo CI_CHECK_CMD: "$CI_CHECK_CMD"
make $CI_MAKE_CHECK_OPT "${CI_CHECK_CMD:-check}"
rc=$?

printf 'STEP: find failing tests ====================================================\n'
echo calling gather-check-logs
devtools/gather-check-logs.sh

## Upload coverage results to Codecov using the official uploader
printf 'STEP: Codecov upload =======================================================\n'
if [ "$CI_CODECOV_TOKEN" != "" ]; then
        coverage_file=""
        ## Prefer lcov for aggregated coverage info; warn if unavailable
        if command -v lcov >/dev/null 2>&1; then
                lcov --capture --directory . --output-file coverage.info --gcov-tool gcov --ignore-errors mismatch \
                 --ignore-errors negative --filter range
                coverage_file="coverage.info"
        else
                printf 'warning: lcov not found; using gcov\n' >&2
                find . -name '*.gcno' -exec gcov -pb {} + >/dev/null 2>&1
        fi
        curl -Os https://uploader.codecov.io/latest/linux/codecov
        curl -Os https://uploader.codecov.io/latest/linux/codecov.SHA256SUM
        if ! sha256sum -c codecov.SHA256SUM; then
                echo 'SHA256 verification for Codecov uploader failed' >&2
                rm -f codecov codecov.SHA256SUM
                exit 1
        else
                chmod +x codecov
                printf "repo slug %s, commit_sha %s\n" "$CODECOV_repo_slug" "$CODECOV_commit_sha"
                if [ -n "$coverage_file" ]; then
                      ./codecov -f "$coverage_file" -C "$CODECOV_commit_sha" -r "$CODECOV_repo_slug" -t "$CI_CODECOV_TOKEN" -n 'rsyslog PR' -Z &> codecov_log
                else
                        ./codecov -C "$CODECOV_commit_sha" -r "$CODECOV_repo_slug" -t "$CI_CODECOV_TOKEN" -n 'rsyslog PR' -Z &> codecov_log
                fi
                cov_rc=$?
                rm codecov codecov.SHA256SUM
                [ -n "$coverage_file" ] && rm "$coverage_file"
                find . -name '*.gcov' -delete
                lines="$(wc -l < codecov_log)"
                if (( lines > 3000 )); then
                        printf 'codecov log file is very large (%d lines), showing parts\n' "$lines"
                        head -n 1500 < codecov_log
                        printf '\n\n... snip ...\n\n'
                        tail -n 1500 < codecov_log
                else
                        cat codecov_log
                fi
                rm codecov_log
                if [ "$rc" -eq 0 ]; then
                        rc=$cov_rc
                fi
        fi
fi

exit $rc
