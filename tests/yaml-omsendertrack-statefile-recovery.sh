#!/bin/bash
# Verify YAML parity for omsendertrack IgnoreInvalidStatefile. A zero-byte
# state file is accepted by the default but rejected when the action opts into
# strict recovery; a clean startup and the strict -N1 diagnostic are the oracle.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init

write_config() {
	local config="$1"
	local statefile="$2"
	local ignore="$3"
	generate_conf
	add_conf '
include(file="'"$config"'")
'
	cat > "$config" <<YAMLEOF
version: 2
modules:
  - load: "../plugins/omsendertrack/.libs/omsendertrack"
rulesets:
  - name: main
    actions:
      - type: omsendertrack
        stateFile: "${statefile}"
        ignoreInvalidStatefile: ${ignore}
YAMLEOF
}

empty_state="${RSYSLOG_DYNNAME}.empty.state"
: > "$empty_state"
write_config "${RSYSLOG_DYNNAME}.recover.yaml" "$empty_state" on
startup
shutdown_immediate
wait_shutdown

strict_state="${RSYSLOG_DYNNAME}.strict.state"
: > "$strict_state"
write_config "${RSYSLOG_DYNNAME}.strict.yaml" "$strict_state" off
if ../tools/rsyslogd -N1 \
	-M"../plugins/omsendertrack/.libs:../plugins/imdiag/.libs:../runtime/.libs:../tools/.libs:../tools" \
	-f"${TESTCONF_NM}.conf" >"${RSYSLOG_DYNNAME}.strict.log" 2>&1; then
	echo "FAIL: YAML strict recovery accepted an empty state file"
	cat "${RSYSLOG_DYNNAME}.strict.log"
	error_exit 1
fi
content_check 'IgnoreInvalidStatefile is off' "${RSYSLOG_DYNNAME}.strict.log"
if [ -s "$strict_state" ]; then
	echo "FAIL: YAML strict recovery modified the empty state file"
	error_exit 1
fi

exit_test
