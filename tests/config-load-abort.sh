#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Intent: when the config itself asks rsyslogd to abort the load, startup
# still fails with exit code 1 and the same stderr message, now that the load
# paths return an error (RS_RET_CONF_LOAD_ABORTED) instead of calling exit().
# Covered requests:
#  - include(mode="abort-if-missing") of a missing file;
#  - failed user lookups with security.abortOnIDResolutionFail (default
#    "on"), via a RainerScript parameter and via a legacy $ directive;
#  - a failed group lookup via a legacy $ directive. (A RainerScript group
#    parameter only aborts on a lookup *error*, not on an unknown group, which
#    cannot be provoked portably, so it is not covered here.)
#
# Stimulus: a normal (non -N) start of rsyslogd in foreground with each
# config; it must not get as far as running.
# Oracle: exit code 1, the expected message on stderr, and no pid file
# (the daemon never got as far as writing it). The run is bounded by "timeout" only so that a regression that
# lets the daemon start does not hang the test; no timing is measured.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "user/group lookup behaviour differs on Solaris"

NOUSER="nouser_${RSYSLOG_DYNNAME}"
CASE_NR=0

# $1 = case description, $2 = expected stderr text; config read from stdin
run_case() {
	CASE_NR=$((CASE_NR+1))
	local conf="${RSYSLOG_DYNNAME}.case$CASE_NR.conf"
	local out="${RSYSLOG_DYNNAME}.case$CASE_NR.out"
	cat > "$conf"
	timeout 60 ../tools/rsyslogd -n -C -f"$conf" -M"$RSYSLOG_MODDIR" \
		-i"${RSYSLOG_DYNNAME}.case$CASE_NR.pid" > "$out" 2>&1
	local rc=$?
	if [ $rc -ne 1 ]; then
		echo "FAIL: case $CASE_NR ($1): exit code $rc, expected 1"
		cat "$out"
		error_exit 1
	fi
	if ! grep -qF -- "$2" "$out"; then
		echo "FAIL: case $CASE_NR ($1): expected message '$2' missing"
		cat "$out"
		error_exit 1
	fi
	if [ -e "${RSYSLOG_DYNNAME}.case$CASE_NR.pid" ]; then
		echo "FAIL: case $CASE_NR ($1): daemon wrote a pid file although the load was aborted"
		cat "$out"
		error_exit 1
	fi
}

run_case "include abort-if-missing" \
	"mode is set to abort-if-missing and the file is indeed missing" <<CONF
include(file="${RSYSLOG_DYNNAME}.does-not-exist.conf" mode="abort-if-missing")
action(type="omfile" file="${RSYSLOG_OUT_LOG}")
CONF

run_case "RainerScript user lookup" "ID for user '${NOUSER}' could not be found" <<CONF
action(type="omfile" file="${RSYSLOG_OUT_LOG}" fileOwner="${NOUSER}")
CONF

run_case "legacy user lookup" "ID for user '${NOUSER}' could not be found or error" <<CONF
\$FileOwner ${NOUSER}
action(type="omfile" file="${RSYSLOG_OUT_LOG}")
CONF

run_case "legacy group lookup" "ID for group '${NOUSER}' could not be found or error" <<CONF
\$FileGroup ${NOUSER}
action(type="omfile" file="${RSYSLOG_OUT_LOG}")
CONF

exit_test
