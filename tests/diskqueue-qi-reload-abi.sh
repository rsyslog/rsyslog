#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# Restart a classic Disk queue from its .qi file on every imdiag testbench,
# including reduced Darwin ARM64 builds. Loading the .qi file calls the
# queue's SETPROPERTY method with an (object, var) pair. The method table
# stores a variadic pointer; an untyped call passes the var on the stack on
# Darwin, and the restarted rsyslogd crashes before it accepts connections.
# The first instance cannot write its output (the directory is missing), so
# every message stays in the action queue and is saved at shutdown. The
# second instance must load that state and deliver the full sequence.
. ${srcdir:=.}/diag.sh init
require_plugin imdiag
export NUMMESSAGES=100
SPOOL_DIR="$PWD/${RSYSLOG_DYNNAME}.spool"
REFERENCE_FILE="$PWD/${RSYSLOG_DYNNAME}.reference.log"
BLOCKED_FILE="$PWD/${RSYSLOG_DYNNAME}.missing/output.log"

write_conf() {
	generate_conf
	add_conf '
global(workDirectory="'"$SPOOL_DIR"'" processInternalMessages="off")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then {
	action(type="omfile" file="'"$1"'" template="outfmt" createDirs="off"
		action.resumeRetryCount="-1"
		queue.type="Disk" queue.filename="qireload"
		queue.saveOnShutdown="on")
	action(type="omfile" file="'"$REFERENCE_FILE"'" template="outfmt")
}
'
}

mkdir -p "$SPOOL_DIR"
write_conf "$BLOCKED_FILE"
startup
injectmsg 0 "$NUMMESSAGES"
# The reference action runs after the enqueue into the disk queue, so a
# full reference file means every message is in that queue.
wait_file_lines "$REFERENCE_FILE" "$NUMMESSAGES"
shutdown_immediate
wait_shutdown
if [ ! -f "$SPOOL_DIR/qireload.qi" ]; then
	printf 'FAIL: no queue state file was written at shutdown\n'
	ls -l "$SPOOL_DIR"
	error_exit 1
fi

write_conf "$RSYSLOG_OUT_LOG"
startup
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 60
shutdown_when_empty
wait_shutdown
# A message in flight at the forced shutdown may be saved and delivered
# again; loss is the failure this test is about.
seq_check 0 $((NUMMESSAGES - 1)) -d
exit_test
