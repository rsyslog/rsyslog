#!/bin/bash
# Verify all imdiag module parameters validate with legacy $-directives disabled
# and the testbench startup path uses module-level serverRun/listenPortFileName.
. ${srcdir:=.}/diag.sh init
require_plugin imdiag

modpath="${RSYSLOG_MODDIR}"

cat >"${RSYSLOG_DYNNAME}.allparams.conf" <<CONF_EOF
global(compatibility.configformat.legacy="disable")
module(load="../plugins/imdiag/.libs/imdiag"
       listenPortFileName="${RSYSLOG_DYNNAME}.allparams.imdiag.port"
       serverRun="0"
       abortTimeout="60"
       injectDelayMode="light"
       maxSessions="10"
       serverStreamDriverMode="0"
       serverStreamDriverAuthMode="anon"
       serverStreamDriverPermittedPeer=["diag.example.com","127.0.0.1"]
       serverInputName="diag-modern"
       mainMsgQueueTimeoutShutdown="10000"
       mainMsgQueueTimeoutEnqueue="30000"
       inputShutdownTimeout="60000"
       defaultActionQueueTimeoutShutdown="20000"
       defaultActionQueueTimeoutEnqueue="30000")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out")
CONF_EOF

../tools/rsyslogd -C -N1 -M"${modpath}" -f"${RSYSLOG_DYNNAME}.allparams.conf" \
	>"${RSYSLOG_DYNNAME}.allparams.log" 2>&1
ret=$?
if [ $ret -ne 0 ]; then
	echo "FAIL: expected imdiag module parameters to validate with legacy disabled"
	cat "${RSYSLOG_DYNNAME}.allparams.log"
	error_exit 1
fi

generate_conf
add_conf '
global(compatibility.configformat.legacy="disable")
'
startup
shutdown_when_empty
wait_shutdown
exit_test
