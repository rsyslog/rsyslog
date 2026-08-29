#!/bin/bash
# Validate RainerScript parsing and Release-B handling of reloadOnHUP. The
# config-check oracle accepts off, validate, and on and rejects invalid values.
# The live daemon oracle sends HUP and requires the side-effect-free candidate
# parser to accept the same RainerScript through the validation-only path. The
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
# This setting would create/truncate the sentinel during ordinary config
# dispatch. Capture-only validation must retain its syntax without executing
# the global setter.
printf '\nglobal(debug.logFile="%s")\n' "$RSYSLOG_DYNNAME.reload-sentinel" >>"$CONF_FILE"
cp "$CONF_FILE" "$CONF_FILE.valid"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=validated_syntax_only active_generation=1"* ]]; then
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
if [[ "$reload_status" != *"result=candidate_syntax_invalid active_generation=1"* ]]; then
	echo "FAIL: unexpected invalid-candidate status: $reload_status"
	error_exit 1
fi
injectmsg 0 1
wait_queueempty

# Restore a valid candidate and prove that lexer/include state was completely
# unwound after the failed attempt.
cp "$CONF_FILE.valid" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=validated_syntax_only active_generation=1"* ]]; then
	echo "FAIL: parser did not recover after invalid candidate: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"
shutdown_when_empty
wait_shutdown
content_check 'shadow_reload event=request result=validated_syntax_only mode=validate'
content_check 'candidate_syntax_invalid'
content_check 'msgnum:00000000'

exit_test
