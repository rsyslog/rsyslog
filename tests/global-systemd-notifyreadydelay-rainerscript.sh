#!/bin/bash
# Validate the global(systemd.notifyReadyDelay=...) operator opt-in. The
# oracle is config validation accepting both binary values, which exercises the
# RainerScript global-parameter backend without depending on a systemd runtime.
. ${srcdir:=.}/diag.sh init

modpath="${RSYSLOG_MODDIR}"

run_expect_success() {
	local cfg="$1"
	local log="$2"

	../tools/rsyslogd -C -N1 -M"${modpath}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if [ $rc -ne 0 ]; then
		echo "FAIL: expected ${cfg} to validate"
		cat "${log}"
		error_exit 1
	fi
}

cat >"${RSYSLOG_DYNNAME}.off.conf" <<CONF_EOF
global(systemd.notifyReadyDelay="off")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.off.conf" "${RSYSLOG_DYNNAME}.off.log"

cat >"${RSYSLOG_DYNNAME}.on.conf" <<CONF_EOF
global(systemd.notifyReadyDelay="on")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF
run_expect_success "${RSYSLOG_DYNNAME}.on.conf" "${RSYSLOG_DYNNAME}.on.log"

exit_test
