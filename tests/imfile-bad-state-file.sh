#!/bin/bash
# Verify that imfile ignores a malformed state file and falls back to a fresh read.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify

generate_conf
add_conf '
global(workDirectory="'${RSYSLOG_DYNNAME}'.spool")
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
      Tag="file:"
      PersistStateInterval="1")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

./inputfilegen -m5 > "$RSYSLOG_DYNNAME.input"
startup
shutdown_when_empty
wait_shutdown
seq_check 0 4

statefile=$(find "$RSYSLOG_DYNNAME.spool" -maxdepth 1 -name 'imfile-state*' | head -n 1)
if [ -z "$statefile" ]; then
	printf 'FAIL: expected imfile state file in %s.spool\n' "$RSYSLOG_DYNNAME"
	ls -la "$RSYSLOG_DYNNAME.spool"
	error_exit 1
fi

printf '{"curr_offs":\n' > "$statefile"
rm -f "$RSYSLOG_OUT_LOG"

startup
shutdown_when_empty
wait_shutdown
seq_check 0 4
content_check "imfile: error reading state file" "$RSYSLOG_DYNNAME.started"

exit_test
