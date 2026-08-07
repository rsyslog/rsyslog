#!/bin/bash
# Regression test for imfile delay.message microsecond conversion.
# A 1000-microsecond delay must deliver one line within 10 seconds. The
# generous bound is not a performance assertion: it tolerates loaded CI hosts
# while deterministically rejecting the old swapped-argument behavior, which
# sleeps for about 1000 seconds.
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
      Tag="file:"
      delay.message="1000")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
touch "$RSYSLOG_DYNNAME.input"
startup
./inputfilegen -m "$NUMMESSAGES" >> "$RSYSLOG_DYNNAME.input"
wait_file_lines "$RSYSLOG_OUT_LOG" "$NUMMESSAGES" 10
shutdown_when_empty
wait_shutdown
seq_check
exit_test
