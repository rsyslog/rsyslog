#!/bin/bash
# added 2026-05-28 by Codex, released under ASL 2.0
#
# Regression coverage for GitHub issue #4724. A RELP/TLS client using
# tcpflood -D randomly closes connections while sending, which used to make
# the imrelp receiver exit silently under flood load. The oracle is that
# tcpflood uses an aggressive random-close threshold, rsyslogd records a broken
# RELP session in its configured internal-message output while it is still
# active, and rsyslogd can still shut down through the normal testbench path.
# The wait on the .started file synchronizes with the omfile action that records
# rsyslogd diagnostics; stdout/stderr alone are not the oracle. Message sequence
# checks are intentionally not used: random client-side disconnects can
# legitimately lose in-flight messages.
. ${srcdir:=.}/diag.sh init
require_plugin imrelp
export NUMMESSAGES=50
generate_conf
add_conf '
global(maxMessageSize="256k" net.enableDNS="off")
main_queue(queue.type="Direct")

module(load="../plugins/imrelp/.libs/imrelp")
input(type="imrelp"
      name="imrelp-tls-randconn-drop"
      address="127.0.0.1"
      port="0"
      tls="on"
      tls.cacert="'$srcdir'/tls-certs/ca.pem"
      tls.mycert="'$srcdir'/tls-certs/cert.pem"
      tls.myprivkey="'$srcdir'/tls-certs/key.pem"
      tls.authmode="certvalid"
      tls.permittedpeer="rsyslog"
      maxDataSize="256k"
      ruleset="discard_relp")

ruleset(name="discard_relp") {
	stop
}
'
startup
assign_single_tcp_listener_port TCPFLOOD_PORT
tcpflood -D -l0.10 -Trelp-tls -acertvalid -p"$TCPFLOOD_PORT" -m"$NUMMESSAGES" \
	-x "$srcdir/tls-certs/ca.pem" \
	-z "$srcdir/tls-certs/key.pem" \
	-Z "$srcdir/tls-certs/cert.pem" \
	-Ersyslog \
	-M "\"<134>1 2026-05-28T00:00:00Z tcpflood tcpflood - - Date is now 2026-05-28T00:00:00Z\"" \
	>"$RSYSLOG_DYNNAME.tcpflood" 2>&1
tcpflood_rc=$?
cat -n "$RSYSLOG_DYNNAME.tcpflood"
if [ "$tcpflood_rc" -ne 0 ]; then
	printf 'tcpflood exited with rc=%s after the intentional random disconnects\n' "$tcpflood_rc"
fi
wait_content "session broken" "$RSYSLOG_DYNNAME.started"

check_rsyslog_active ""

shutdown_when_empty
wait_shutdown

cat "$RSYSLOG_DYNNAME.tcpflood" "$RSYSLOG_DYNNAME.started" >"$RSYSLOG_DYNNAME.disconnect.log"
content_check "session broken" "$RSYSLOG_DYNNAME.disconnect.log"
exit_test
