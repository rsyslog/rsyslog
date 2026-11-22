#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML twin of config-load-abort.sh for the case YAML can express: an omfile
# action whose fileOwner names a user that does not exist, with
# security.abortOnIDResolutionFail at its default "on". (YAML has no
# include mode "abort-if-missing" and no legacy $ directives.)
#
# Intent: the YAML loader reaches the same lookup, the load fails with
# RS_RET_CONF_LOAD_ABORTED instead of exit(), and startup still exits 1.
# Stimulus: a normal (non -N) foreground start with a .yaml main config.
# Oracle: exit code 1, the lookup message on stderr, and no pid file. The
# "timeout" only bounds a regression that lets the daemon start.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "user/group lookup behaviour differs on Solaris"

NOUSER="nouser_${RSYSLOG_DYNNAME}"
CONF="${RSYSLOG_DYNNAME}.yaml"
OUT="${RSYSLOG_DYNNAME}.out"
PIDF="${RSYSLOG_DYNNAME}.case.pid"

cat > "$CONF" <<YAMLEOF
rulesets:
  - name: RSYSLOG_DefaultRuleset
    actions:
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        fileOwner: "${NOUSER}"
YAMLEOF

timeout 60 ../tools/rsyslogd -n -C -f"$CONF" -M"$RSYSLOG_MODDIR" -i"$PIDF" > "$OUT" 2>&1
rc=$?
if [ $rc -ne 1 ]; then
	echo "FAIL: exit code $rc, expected 1"
	cat "$OUT"
	error_exit 1
fi
if ! grep -qF -- "ID for user '${NOUSER}' could not be found" "$OUT"; then
	echo "FAIL: expected lookup message missing"
	cat "$OUT"
	error_exit 1
fi
if [ -e "$PIDF" ]; then
	echo "FAIL: daemon wrote a pid file although the load was aborted"
	cat "$OUT"
	error_exit 1
fi
exit_test
