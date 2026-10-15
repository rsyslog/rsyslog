#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Validate RainerScript parsing and Release-B handling of reloadOnHUP. The
# config-check oracle accepts off, validate, and on and rejects invalid values.
# The live daemon oracle then sends HUP and requires validate to log an explicit
# unsupported rejection; it must not claim that any candidate was validated.
# Reset discards a deferred main_queue without unknown-parameter diagnostics;
# the same unknown parameter without reset must still fail end-load validation.
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

cfg="${RSYSLOG_DYNNAME}.reset-mainqueue.conf"
cat >"${cfg}" <<CONF_EOF
main_queue(queue.type="LinkedList" queue.size="1000")
\$ResetConfigVariables
template(name="resetFormat" type="string" string="%msg%")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out" template="resetFormat")
CONF_EOF
run_expect_status 0 "${cfg}" "${RSYSLOG_DYNNAME}.reset-mainqueue.log"
if grep -q 'not known' "${RSYSLOG_DYNNAME}.reset-mainqueue.log"; then
	cat "${RSYSLOG_DYNNAME}.reset-mainqueue.log"
	error_exit 1
fi
cfg="${RSYSLOG_DYNNAME}.unknown-mainqueue.conf"
cat >"${cfg}" <<CONF_EOF
main_queue(queue.unknownResetControl="1")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_status 1 "${cfg}" "${RSYSLOG_DYNNAME}.unknown-mainqueue.log"
content_check 'not known' "${RSYSLOG_DYNNAME}.unknown-mainqueue.log"

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
issue_HUP
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'shadow_reload event=request result=rejected mode=validate'
content_check 'rejected_mode=validate rejected_reason=unsupported_release_b'
assert_content_missing 'result=validated'

exit_test
