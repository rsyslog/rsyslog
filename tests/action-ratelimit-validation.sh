#!/bin/bash
# Validate generic output rate-limit configuration errors with rsyslogd -N1.
# The oracle is that each invalid configuration fails before startup: actions
# cannot reference input-scoped policies, output policies cannot use inline
# tunables or omit required YAML limits, pace mode needs usable burst capacity,
# and pace mode cannot run on a direct action queue.
. ${srcdir:=.}/diag.sh init

check_invalid() {
	conf="$1"
	../tools/rsyslogd -u2 -N1 -f"$conf" -M../runtime/.libs:../.libs
	rc=$?
	if [ "$rc" -eq 0 ]; then
		echo "FAIL: expected invalid config $conf to fail validation"
		error_exit 1
	fi
}

INPUT_POLICY="$(pwd)/${RSYSLOG_DYNNAME}.input-policy.yaml"
OUTPUT_DROP_POLICY="$(pwd)/${RSYSLOG_DYNNAME}.output-drop-policy.yaml"
OUTPUT_MISSING_LIMIT_POLICY="$(pwd)/${RSYSLOG_DYNNAME}.output-missing-limit-policy.yaml"
OUTPUT_PACE_POLICY="$(pwd)/${RSYSLOG_DYNNAME}.output-pace-policy.yaml"
OUTPUT_PACE_ZERO_POLICY="$(pwd)/${RSYSLOG_DYNNAME}.output-pace-zero-policy.yaml"

cat > "$INPUT_POLICY" << 'YAMLEOF'
interval: 60
burst: 10
YAMLEOF

cat > "$OUTPUT_DROP_POLICY" << 'YAMLEOF'
scope: output
mode: drop
interval: 60
burst: 10
YAMLEOF

cat > "$OUTPUT_MISSING_LIMIT_POLICY" << 'YAMLEOF'
scope: output
mode: drop
YAMLEOF

cat > "$OUTPUT_PACE_POLICY" << 'YAMLEOF'
scope: output
mode: pace
interval: 1
burst: 1
YAMLEOF

cat > "$OUTPUT_PACE_ZERO_POLICY" << 'YAMLEOF'
scope: output
mode: pace
interval: 1
burst: 0
YAMLEOF

cat > "${RSYSLOG_DYNNAME}.input-used-by-action.conf" << CONFEOF
ratelimit(name="input_only" policy="$INPUT_POLICY")
action(type="omfile" file="$RSYSLOG_OUT_LOG" action.ratelimit.name="input_only")
CONFEOF
check_invalid "${RSYSLOG_DYNNAME}.input-used-by-action.conf"

cat > "${RSYSLOG_DYNNAME}.output-inline.conf" << CONFEOF
ratelimit(name="bad_output" policy="$OUTPUT_DROP_POLICY" interval="60")
action(type="omfile" file="$RSYSLOG_OUT_LOG" action.ratelimit.name="bad_output")
CONFEOF
check_invalid "${RSYSLOG_DYNNAME}.output-inline.conf"

cat > "${RSYSLOG_DYNNAME}.output-missing-limit.conf" << CONFEOF
ratelimit(name="bad_output_missing_limit" policy="$OUTPUT_MISSING_LIMIT_POLICY")
action(type="omfile" file="$RSYSLOG_OUT_LOG" action.ratelimit.name="bad_output_missing_limit")
CONFEOF
check_invalid "${RSYSLOG_DYNNAME}.output-missing-limit.conf"

cat > "${RSYSLOG_DYNNAME}.pace-zero-burst.conf" << CONFEOF
ratelimit(name="out_pace_zero" policy="$OUTPUT_PACE_ZERO_POLICY")
action(type="omfile" file="$RSYSLOG_OUT_LOG" queue.type="linkedlist" action.ratelimit.name="out_pace_zero")
CONFEOF
check_invalid "${RSYSLOG_DYNNAME}.pace-zero-burst.conf"

cat > "${RSYSLOG_DYNNAME}.pace-direct.conf" << CONFEOF
ratelimit(name="out_pace" policy="$OUTPUT_PACE_POLICY")
action(type="omfile" file="$RSYSLOG_OUT_LOG" action.ratelimit.name="out_pace")
CONFEOF
check_invalid "${RSYSLOG_DYNNAME}.pace-direct.conf"

exit_test
