#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2026 Jérémie Jourdin.
#
# freshStartTail seeks to EOF without reading, so the read buffer stays empty
# and the offset is the original size. An in-place shrink then leaves that
# offset past EOF while rsyslog is still running, with no state file involved.
# checkTruncation must notice from the file size alone; without the
# empty-buffer branch this test hangs in wait_file_lines.
# The per-file stats object is registered just before the first poll, which
# performs the freshStartTail seek, so the impstats line naming this input
# marks the seek. The input is then replaced in place by two lines.
. ${srcdir:=.}/diag.sh init
require_plugin impstats

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile" mode="polling" PollingInterval="1")
module(load="../plugins/impstats/.libs/impstats"
	log.file="./'$RSYSLOG_DYNNAME'.stats.log"
	log.syslog="off"
	interval="1")
input(type="imfile" File="./'$RSYSLOG_DYNNAME'.input" Tag="file:"
	freshStartTail="on" reopenOnTruncate="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

./inputfilegen -m 80 > "$RSYSLOG_DYNNAME.input"
startup
# Registered immediately before the first pollFile(), which seeks to EOF.
wait_content "$RSYSLOG_DYNNAME.input" "$RSYSLOG_DYNNAME.stats.log"
if [ -s "$RSYSLOG_OUT_LOG" ]; then
	printf 'FAIL: freshStartTail read lines that were already in the file\n'
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

# Same inode. The replacement is much smaller than the offset freshStartTail
# stored, so a reader that does not look at the size stays past EOF.
: > "$RSYSLOG_DYNNAME.input"
./inputfilegen -m 2 > "$RSYSLOG_DYNNAME.input"
wait_file_lines "$RSYSLOG_OUT_LOG" 2 50
shutdown_when_empty
wait_shutdown
seq_check 0 1
exit_test
