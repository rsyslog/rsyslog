#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML parity for construction-time rejection of malformed input arrays and
# omfwd's comma-separated peer list. Best-effort startup must keep both an
# immediately invalid input and a valid-prefix/invalid-tail input disabled.
# Legacy-only directives in a YAML ruleset script must also retain parse
# failures across subsequent peers. The healthy YAML listener
# supplies the positive startup/delivery control.
# Rejected port files stay absent; diagnostic counts use configured omfile
# output after synchronized shutdown, with no timing-based negative assertion.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
include(file="'${TESTCONF_NM}'.yaml")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
add_yaml_conf '
global:
  AbortOnUncleanConfig: "off"
  inputStartupPolicy: "best-effort"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
    StreamDriver.Name: ossl
    StreamDriver.Mode: "1"
    StreamDriver.AuthMode: x509/name
    StreamDriver.CAFile: "'$srcdir'/tls-certs/ca.pem"
    StreamDriver.CertFile: "'$srcdir'/tls-certs/cert.pem"
    StreamDriver.KeyFile: "'$srcdir'/tls-certs/key.pem"
    PermittedPeer: ["*.com"]
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.invalid-first.port"
    PermittedPeer: ["bad*middle.invalid"]
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.invalid-tail.port"
    PermittedPeer: ["valid.example.com", "*.valid.bad*middle"]
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"
rulesets:
  - name: legacy-rejected
    script: |
      $ActionSendStreamDriver gtls
      $ActionSendStreamDriverMode 1
      $ActionSendStreamDriverAuthMode x509/name
      $ActionSendStreamDriverPermittedPeer bad*middle.invalid
      :msg, contains, "never-forward" @@127.0.0.1:1
      $ActionSendStreamDriverPermittedPeer valid.example.com
      :msg, contains, "never-forward" @@127.0.0.1:1
  - name: rejected
    actions:
      - type: omfwd
        target: "127.0.0.1"
        port: "1"
        protocol: tcp
        StreamDriverPermittedPeers: "valid.example.com,*.valid.bad*middle"
'
startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
tcpflood -m1 -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty
wait_shutdown
content_check "msgnum:"
content_check_with_count "error compiling wildcard expression" 4
content_check_with_count "legacy TCP action rejected" 2
for port_file in "$RSYSLOG_DYNNAME.invalid-first.port" "$RSYSLOG_DYNNAME.invalid-tail.port"; do
    if [ -e "$port_file" ]; then
        echo "invalid YAML permitted-peer input activated: $port_file"
        error_exit 1
    fi
done
exit_test
