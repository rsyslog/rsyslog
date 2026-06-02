#!/bin/bash
# Validate YAML parity for the modern global maxOpenFiles setting.  The config
# check includes a YAML singleton global section and verifies that the shared
# global-parameter backend accepts the value without activating setrlimit.
. ${srcdir:=.}/diag.sh init

modpath="${RSYSLOG_MODDIR}"

cat >"${RSYSLOG_DYNNAME}.conf" <<CONF_EOF
include(file="${RSYSLOG_DYNNAME}.yaml")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF

cat >"${RSYSLOG_DYNNAME}.yaml" <<YAML_EOF
global:
  maxOpenFiles: 4096
YAML_EOF

../tools/rsyslogd -C -N1 -M"${modpath}" -f"${RSYSLOG_DYNNAME}.conf" >"${RSYSLOG_DYNNAME}.log" 2>&1
rc=$?
if [ $rc -ne 0 ]; then
	echo "FAIL: expected YAML global maxOpenFiles to validate"
	cat "${RSYSLOG_DYNNAME}.log"
	error_exit 1
fi

exit_test
