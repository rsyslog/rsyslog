#!/bin/bash
# Verify that a YAML top-level config can include legacy imgssapi directives
# and still exercise the listenPortFileName forwarding path.
. ${srcdir:=.}/diag.sh init
require_plugin imgssapi

count_imgssapi_msgs() {
	if [ -f "$RSYSLOG_DYNNAME.msgs.log" ]; then
		grep -c 'msgnum:' "$RSYSLOG_DYNNAME.msgs.log"
	else
		echo 0
	fi
}

wait_imgssapi_msgs() {
	wait_file_lines --count-function count_imgssapi_msgs "$RSYSLOG_DYNNAME.msgs.log" 10
}

export QUEUE_EMPTY_CHECK_FUNC=wait_imgssapi_msgs

generate_conf --yaml-only

cat > "${RSYSLOG_DYNNAME}_imgssapi.conf" << EOF
template(name="outfmt" type="string" string="%msg%\n")
module(load="../plugins/imgssapi/.libs/imgssapi")
\$InputGSSServerPermitPlainTCP on
\$InputGSSServerTokenIOTimeout 10
\$InputGSSListenPortFileName ${RSYSLOG_DYNNAME}.gss_port
\$InputGSSServerRun 0
action(type="omfile" file="${RSYSLOG_DYNNAME}.msgs.log" template="outfmt")
EOF

add_yaml_conf 'include:'
add_yaml_conf '  - path: "'${RSYSLOG_DYNNAME}'_imgssapi.conf"'
add_yaml_conf ''
add_yaml_conf 'inputs:'
add_yaml_imdiag_input

startup

assign_file_content GSS_PORT "$RSYSLOG_DYNNAME.gss_port"
if [ -z "$GSS_PORT" ] || [ "$GSS_PORT" = "0" ]; then
	echo "FAIL: expected dynamic GSS port file to contain a bound port, got '$GSS_PORT'"
	exit 1
fi

export TCPFLOOD_PORT="$GSS_PORT"
tcpflood -m10 -i1
shutdown_when_empty
wait_shutdown

content_count_check "msgnum:" 10 "$RSYSLOG_DYNNAME.msgs.log"
exit_test
