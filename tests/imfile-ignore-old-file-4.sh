#! /bin/bash
# set ignoreolderthan to zero
echo [imfile-ignore-old-file-4.sh]
. $srcdir/diag.sh check-inotify
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
      ignoreolderthan="0"
      Tag="file:"
      ruleset="ruleset")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="ruleset") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
# create a log for testing
./inputfilegen -m 1000 > ${RSYSLOG_DYNNAME}.input
touch -m -t 201806010000.00 ./${RSYSLOG_DYNNAME}.input
startup
# sleep a little to give rsyslog a chance to begin processing
./msleep 500
# shut down rsyslogd when done processing messages
shutdown_when_empty 
# we need to wait until rsyslogd is finished!
wait_shutdown    
# check log file
seq_check 0 999
exit_test
