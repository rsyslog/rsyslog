#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Reject malformed wildcard lists during construction, before any TLS match.
# Both input and output configuration paths must report compilation failure.
# A valid earlier peer plus a valid wildcard prefix exercises partial cleanup.
# Assert rsyslog's configured output after synchronized shutdown.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp"
       PermittedPeer=["valid.example.com","*.valid.bad*middle"])
action(type="omfwd" target="127.0.0.1" port="1" protocol="tcp"
       StreamDriverPermittedPeers="valid.example.com,*.valid.bad*middle")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check_with_count "error compiling wildcard expression" 2
exit_test
