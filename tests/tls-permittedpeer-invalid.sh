#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject malformed wildcard lists during construction, before any TLS match.
# Input, modern output, and legacy output paths must report construction errors.
# A failed legacy restriction stays sticky across a later valid peer and is
# rejected actions emit two diagnostics instead of dropping the restriction.
# A valid earlier peer plus a valid wildcard prefix exercises partial cleanup.
# Assert rsyslog's configured output after synchronized shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp"
       PermittedPeer=["valid.example.com","*.valid.bad*middle"])
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.invalid-module.port")
action(type="omfwd" target="127.0.0.1" port="1" protocol="tcp"
       StreamDriverPermittedPeers="valid.example.com,*.valid.bad*middle")
$ActionSendStreamDriver gtls
$ActionSendStreamDriverMode 1
$ActionSendStreamDriverAuthMode x509/name
$ActionSendStreamDriverPermittedPeer bad*middle.invalid
:msg, contains, "never-forward" @@127.0.0.1:1
$ActionSendStreamDriverPermittedPeer valid.example.com
:msg, contains, "never-forward" @@127.0.0.1:1
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check_with_count "error compiling wildcard expression" 3
content_check_with_count "legacy TCP action rejected" 2
if [ -e "$RSYSLOG_DYNNAME.invalid-module.port" ]; then
    echo "input with an invalid module peer list activated"
    error_exit 1
fi
exit_test
