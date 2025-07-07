#! /bin/bash
# [polling mode] Multiple files with different timestamp, ignore those files with old timestamp

echo [imfile-ignore-old-file-6.sh]

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile"
		mode="polling"
       	pollingInterval="2"
       )
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input.1"
      ignoreolderthan="604800"
      Tag="file:"
      ruleset="ruleset")
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input.2"
      ignoreolderthan="604800"
      Tag="file:"
      ruleset="ruleset")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="ruleset") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
# create log files for testing
./inputfilegen -m 1000 > $RSYSLOG_DYNNAME.input.1
touch -m -t 201806010000.00 ./$RSYSLOG_DYNNAME.input.1
./inputfilegen -m 1000 -i 1000 > $RSYSLOG_DYNNAME.input.2

startup
# sleep a little to give rsyslog a chance to begin processing
./msleep 500
# shut down rsyslogd when done processing messages
shutdown_when_empty 
# we need to wait until rsyslogd is finished!
wait_shutdown    
# check log file
seq_check 1000 1999
exit_test
