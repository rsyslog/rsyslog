#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Best-effort startup must not activate an invalid permitted-peer override with
# inherited module peers, nor with the valid prefix of a partially built list.
# A healthy listener proves that startup completed. After synchronized shutdown,
# rejected inputs must have no published port files and both errors must reach
# configured omfile output. Port 0 avoids port preselection races.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
global(AbortOnUncleanConfig="off" inputStartupPolicy="best-effort"
       defaultNetstreamDriverCAFile="'$srcdir'/tls-certs/ca.pem"
       defaultNetstreamDriverCertFile="'$srcdir'/tls-certs/cert.pem"
       defaultNetstreamDriverKeyFile="'$srcdir'/tls-certs/key.pem")
module(load="../plugins/imtcp/.libs/imtcp" StreamDriver.Name="ossl" StreamDriver.Mode="1"
       StreamDriver.AuthMode="x509/name" PermittedPeer=["*.com"])
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.invalid-first.port"
      PermittedPeer=["bad*middle.invalid"])
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.invalid-tail.port"
      PermittedPeer=["valid.example.com","*.valid.bad*middle"])
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood -m1 -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
content_check "msgnum:"
content_check_with_count "error compiling wildcard expression" 2
for port_file in "$RSYSLOG_DYNNAME.invalid-first.port" "$RSYSLOG_DYNNAME.invalid-tail.port"; do
    if [ -e "$port_file" ]; then
        echo "invalid permitted-peer input activated: $port_file"
        error_exit 1
    fi
done
exit_test
