#!/bin/bash
# add 2026-06-02 by OpenAI Codex
# Regression test for issues #2524 and #2568.  An explicit "continue" inside
# an if branch is a user-requested NOP, and the optimizer must not make a
# strict AbortOnUncleanConfig validation run fail or report the historical
# "we see a NOP" diagnostic.  Success is proven by rsyslogd -N1 exiting
# cleanly and by checking its captured validation output.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
global(abortOnUncleanConfig="on")

if re_match($msg, "whitelist.*") then {
	continue
} else {
	stop
}
'

out="${RSYSLOG_DYNNAME}.config-check.log"
../tools/rsyslogd -C -N1 -f"${TESTCONF_NM}.conf" -M../runtime/.libs:../.libs >"$out" 2>&1
if [ $? -ne 0 ]; then
	echo "Error: config check failed for intentional continue/NOP"
	cat "$out"
	error_exit 1
fi

check_not_present "optimizer error: we see a NOP" "$out"
check_not_present 'global(AbortOnUncleanConfig="on") is set' "$out"

exit_test
