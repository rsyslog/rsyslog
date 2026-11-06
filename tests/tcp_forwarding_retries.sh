#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# added 2016-06-21 by RGerhards, released under ASL 2.0
# Reserve the receiver port before starting rsyslog, but hold listen() until
# omfwd reports a real connection refusal. This preserves retry coverage while
# preventing imdiag from acquiring the temporarily unused forwarding port.
# Stderr is only an early release signal: the synchronous forwarding action
# blocks configured internal-message output while it retries, so that normal
# output cannot release the listener in time. This is a diagnostic-sink exception;
# exact delivery of messages 0..19999 remains the primary success oracle.
. ${srcdir:=.}/diag.sh init

messages=20000

generate_conf
LISTEN_RELEASE="$RSYSLOG_DYNNAME.forward-listen-release"
LISTEN_READY="$RSYSLOG_DYNNAME.forward-listen-ready"
ACCEPT_READY="$RSYSLOG_DYNNAME.forward-accept-ready"
start_minitcpsrvr "$RSYSLOG_OUT_LOG" 1 "$LISTEN_RELEASE" "$LISTEN_READY" "$ACCEPT_READY"

add_conf '
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfwd"
	       target="127.0.0.1" port="'$MINITCPSRVR_PORT1'" protocol="TCP"
	       action.resumeRetryCount="10"
	       action.resumeInterval="1" pool.resumeInterval="1"
	       template="outfmt")
}
'

# One-second retry intervals allow recovery from the intentional refusal
# within the normal action-queue shutdown window, without a fixed startup sleep.
export RS_REDIR=">${RSYSLOG_DYNNAME}.sender.stderr 2>&1"
startup
injectmsg 0 $messages
wait_content "cannot connect to 127\.0\.0\.1:$MINITCPSRVR_PORT1 " "$RSYSLOG_DYNNAME.sender.stderr"
: > "$LISTEN_RELEASE"
wait_file_exists "$LISTEN_READY"
wait_file_exists "$ACCEPT_READY"
shutdown_when_empty
wait_shutdown
stop_minitcpsrvrs

seq_check 0 $(($messages-1))
exit_test
