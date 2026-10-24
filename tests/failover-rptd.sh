#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify repeated-message failover preserves all 5000 messages. The helper
# owns an ephemeral TCP port without listening, so the primary stays unavailable
# even when other tests start listeners concurrently. Its port file proves bind
# completed; the release file is never created. Normal test cleanup stops it,
# and seq_check after queue-empty shutdown checks every fallback record.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
NUMMESSAGES=5000
generate_conf
MINITCPSRV_EXTRA_OPTS="-E" start_minitcpsrvr "$RSYSLOG_DYNNAME.unavailable-target.log" 1 \
	"$RSYSLOG_DYNNAME.unavailable-target.release"
add_conf '
$RepeatedMsgReduction on

$template outfmt,"%msg:F,58:2%\n"
# note: the target server shall not be available!
:msg, contains, "msgnum:" @@127.0.0.1:'$MINITCPSRVR_PORT1'
$ActionExecOnlyWhenPreviousIsSuspended on
& ./'$RSYSLOG_OUT_LOG';outfmt
'
startup
injectmsg 0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown 
seq_check
exit_test
