#!/bin/bash
# Validate RainerScript parsing and Release-B handling of reloadOnHUP. The
# config-check oracle accepts off, validate, and on and rejects invalid values.
# The live daemon oracle sends HUP and requires the side-effect-free candidate
# parser to report the same RainerScript through the source-diff-only path. The
# active generation remains unchanged because this step does not activate.
. ${srcdir:=.}/diag.sh init

modpath="${RSYSLOG_MODDIR}"

run_expect_status() {
	local expected="$1"
	local cfg="$2"
	local log="$3"

	../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if [ "${rc}" -ne "${expected}" ]; then
		echo "FAIL: expected ${cfg} to exit ${expected}, got ${rc}"
		cat "${log}"
		error_exit 1
	fi
}

for mode in off validate on; do
	cfg="${RSYSLOG_DYNNAME}.${mode}.conf"
	cat >"${cfg}" <<CONF_EOF
global(config.reloadOnHUP="${mode}")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
	run_expect_status 0 "${cfg}" "${RSYSLOG_DYNNAME}.${mode}.log"
done

cfg="${RSYSLOG_DYNNAME}.invalid.conf"
cat >"${cfg}" <<CONF_EOF
global(config.reloadOnHUP="invalid")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_status 1 "${cfg}" "${RSYSLOG_DYNNAME}.invalid.log"
content_check "invalid config.reloadOnHUP value 'invalid'; expected off, validate, or on" \
	"${RSYSLOG_DYNNAME}.invalid.log"

generate_conf
add_conf '
global(processInternalMessages="on" config.reloadOnHUP="validate")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
# An unchanged candidate must produce a pure source-diff NOOP before the
# later sentinel mutation exercises a targeted global-object modification.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1"* ]]; then
	echo "FAIL: unchanged candidate did not validate report-only: $reload_status"
	error_exit 1
fi
# This setting would create/truncate the sentinel during ordinary config
# dispatch. Capture-only validation must retain its syntax without executing
# the global setter.
printf '\nglobal(debug.logFile="%s")\n' "$RSYSLOG_DYNNAME.reload-sentinel" >>"$CONF_FILE"
cp "$CONF_FILE" "$CONF_FILE.valid"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"

# A malformed replacement must produce a terminal rejection without changing
# the live generation. issue_HUP's generation acknowledgement is the
# deterministic completion oracle; the injected record proves continued use
# of the old action after rollback.
printf 'global(config.reloadOnHUP="validate"\n' >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_parse_invalid active_generation=1"* ]]; then
	echo "FAIL: unexpected invalid-candidate status: $reload_status"
	error_exit 1
fi
injectmsg 0 1
wait_queueempty

# A required missing include is an I/O error, never a reason for lower-level
# code to terminate the process. During reload it must remain candidate-local;
# the live PID and old action stay available. The HUP acknowledgement is the
# deterministic proof that parsing returned through the reload manager.
printf 'global(config.reloadOnHUP="validate")\ninclude(file="%s" mode="abort-if-missing")\n' \
	"$RSYSLOG_DYNNAME.missing-reload-include" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_io_error active_generation=1"* ]]; then
	echo "FAIL: missing mandatory include was not reported as an I/O rejection: $reload_status"
	error_exit 1
fi
injectmsg 1 1
wait_queueempty

# Backticks read the process environment before the candidate's declarative
# global(environment=...) could be applied privately. Until a private overlay
# exists, fail closed instead of producing a context-dependent graph.
cat >"$CONF_FILE" <<'CONF_EOF'
global(config.reloadOnHUP="validate")
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
CONF_EOF
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_parse_invalid active_generation=1"* ]]; then
	echo "FAIL: candidate backtick expansion was not rejected: $reload_status"
	error_exit 1
fi
injectmsg 2 1
wait_queueempty

# Duplicate generation-global action identities make the normalized report
# invalid. This is distinct from a parse failure and must remain a fail-closed
# control-plane outcome; the active action still processes the next record.
cat >"$CONF_FILE" <<CONF_EOF
global(config.reloadOnHUP="validate")
ruleset(name="first") {
    action(type="omfile" name="duplicate" file="$RSYSLOG2_OUT_LOG")
}
ruleset(name="second") {
    action(type="omfile" name="duplicate" file="$RSYSLOG2_OUT_LOG")
}
CONF_EOF
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_report_invalid active_generation=1"* ]]; then
	echo "FAIL: duplicate action identity did not invalidate the report: $reload_status"
	error_exit 1
fi
injectmsg 3 1
wait_queueempty

# Restore a valid candidate and prove that lexer/include state was completely
# unwound after the failed attempt.
cp "$CONF_FILE.valid" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1"* ]]; then
	echo "FAIL: parser did not recover after invalid candidate: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"

# Losing access to the master file is an I/O rejection, not a syntax error.
# issue_HUP acknowledges the complete legacy-HUP and reload-manager cycle.
mv "$CONF_FILE" "$CONF_FILE.unavailable"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_io_error active_generation=1"* ]]; then
	echo "FAIL: unavailable candidate was not reported as an I/O error: $reload_status"
	error_exit 1
fi
mv "$CONF_FILE.unavailable" "$CONF_FILE"
shutdown_when_empty
wait_shutdown
content_check 'shadow_reload event=request result=reported_only mode=validate candidate_objects=6 unchanged=4 added=0 removed=0 modified=1 invalid=0'
content_check 'shadow_reload event=request result=reported_only mode=validate candidate_objects=5 unchanged=5 added=0 removed=0 modified=0 invalid=0'
content_check 'candidate_parse_invalid'
content_check 'msgnum:00000000'
content_check 'msgnum:00000001'
content_check 'msgnum:00000002'
content_check 'msgnum:00000003'

exit_test
