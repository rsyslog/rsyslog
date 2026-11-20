#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0
# Shared body of omfwd-tls-{gtls,ossl}-close-notify.sh; the caller sets
# TLS_DRIVER to the netstream driver under test.
#
# Regression test for https://github.com/rsyslog/rsyslog/issues/1503:
# a TLS server that closes an idle connection sends a close_notify alert
# before its FIN. The unread alert kept the plain TCP CheckConnection()
# peek from seeing the close, so omfwd wrote the next message into the
# dead connection and lost it.
#
# Setup: omfwd forwards over TLS to omfwd-tls-close-notify-server.py,
# which closes the first connection with close_notify right after it
# received the first message and then writes a "closed" marker file.
# Stimulus: inject message 0, wait for the marker, inject message 1.
# Oracle: both messages arrive at the server. Without the fix message 1
# is written into the closed connection and never shows up. The wait for
# the marker file orders the second message after the alert; no timing
# assumption is involved beyond that.
# The server runs in the background. It binds port 0 and writes the port
# file only after listen(2), so wait_file_exists is the readiness oracle;
# an EXIT trap kills it.
. ${srcdir:=.}/diag.sh init

PORT_FILE="$RSYSLOG_DYNNAME.cn-server.port"
CLOSED_FILE="$RSYSLOG_DYNNAME.cn-server.closed"
SERVER_OUT="$RSYSLOG_DYNNAME.cn-server.out"

$PYTHON "$srcdir/omfwd-tls-close-notify-server.py" "$PORT_FILE" \
	"$srcdir/tls-certs/cert.pem" "$srcdir/tls-certs/key.pem" \
	"$SERVER_OUT" "$CLOSED_FILE" &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true' EXIT
wait_file_exists "$PORT_FILE"
PORT=$(cat "$PORT_FILE")

generate_conf
add_conf '
global(DefaultNetstreamDriverCAFile="'$srcdir/tls-certs/ca.pem'")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")

:msg, contains, "msgnum:" {
	action(type="omfwd" target="127.0.0.1" port="'$PORT'" protocol="tcp"
	       template="outfmt"
	       StreamDriver="'$TLS_DRIVER'" StreamDriverMode="1"
	       StreamDriverAuthMode="anon")
}
'
startup
injectmsg 0 1
wait_file_exists "$CLOSED_FILE"
injectmsg 1 1
wait_file_lines "$SERVER_OUT" 2
shutdown_when_empty
wait_shutdown

export SEQ_CHECK_FILE="$SERVER_OUT"
seq_check 0 1
exit_test
