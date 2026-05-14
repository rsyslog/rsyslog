#!/bin/bash
# added 2026-05-12 to guard issue #5636, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imrelp
require_plugin impstats
require_plugin omrelp
require_relpEngineSetTLSLibByName

export NUMMESSAGES=50
export TB_TEST_MAX_RUNTIME=60
export PORT_RCVR="$(get_free_port)"
export STATSFILE="$RSYSLOG_DYNNAME.stats"

generate_conf
add_conf '
module(load="../plugins/imrelp/.libs/imrelp" tls.tlslib="openssl")

input(type="imrelp"
	port="'$PORT_RCVR'"
	tls="on"
	tls.cacert="'$srcdir'/tls-certs/ca.pem"
	tls.mycert="'$srcdir'/tls-certs/cert.pem"
	tls.myprivkey="'$srcdir'/tls-certs/key.pem")

*.* action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup

generate_conf 2
add_conf '
module(load="../plugins/impstats/.libs/impstats" log.file="'$STATSFILE'" interval="1")
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="openssl")

action(type="omrelp"
	name="issue_5636_omrelp"
	target="127.0.0.1"
	port="'$PORT_RCVR'"
	tls="on"
	tls.permanentFailureDisablesAction="off"
	action.resumeRetryCount="0"
	action.resumeInterval="60"
	queue.type="LinkedList")
action(type="omfile" file="'$RSYSLOG_DYNNAME.errmsgs'")
' 2
startup 2

injectmsg2 1 $NUMMESSAGES
sleep 2

content_check "suspending action" "$RSYSLOG_DYNNAME.errmsgs"
if grep -q "DISABLING action" "$RSYSLOG_DYNNAME.errmsgs"; then
	echo "FAIL: omrelp disabled action despite tls.permanentFailureDisablesAction=off"
	error_exit 1
fi

content_check --regex \
	"issue_5636_omrelp queue: origin=core.queue .*enqueued=[5-9][0-9]" \
	"$STATSFILE"

shutdown_immediate 2
wait_shutdown 2 30
shutdown_immediate
wait_shutdown "" 30

exit_test
