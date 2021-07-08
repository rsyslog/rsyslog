#! /bin/bash
# [polling mode] Don't read a file with old timestamp
# touch the file, then read it

echo [imfile-ignore-old-file-5.sh]
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile"
    mode="polling"
    pollingInterval="1"
    )
input(type="imfile"
      File="./'$RSYSLOG_DYNNAME'.input"
      ignoreolderthan="604800"
      Tag="file:"
      ruleset="ruleset")
template(name="outfmt" type="string" string="%msg:F,58:2%\n")
ruleset(name="ruleset") {
  action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
# create a log for testing
./inputfilegen -m 1000 > $RSYSLOG_DYNNAME.input
touch -m -t 201806010000.00 ./$RSYSLOG_DYNNAME.input

startup
# sleep a little to give rsyslog a chance to begin processing
./msleep 500

if ls ${RSYSLOG_OUT_LOG} > /dev/null; then
	echo "FAIL: rsyslog.out.log isn't expected to be generated!"
	exit 1
fi

# change timestamp
./inputfilegen -m 1000 -i 1000 >> $RSYSLOG_DYNNAME.input
ls -l ./$RSYSLOG_DYNNAME.input

./msleep 1000
# shut down rsyslogd when done processing messages
shutdown_when_empty 
# we need to wait until rsyslogd is finished!
wait_shutdown    
# check log file
seq_check 0 1999
exit_test
