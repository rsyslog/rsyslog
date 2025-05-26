#! /bin/bash
# Don't read a file with old timestamp
echo [imfile-ignore-old-file-1.sh]
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
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
./inputfilegen -m 1000 > ${RSYSLOG_DYNNAME}.input

touch -m -t 201806010000.00 ./${RSYSLOG_DYNNAME}.input

startup

# shut down rsyslogd when done processing messages
shutdown_when_empty 
# we need to wait until rsyslogd is finished!
wait_shutdown    

if ls ${RSYSLOG_OUT_LOG} > /dev/null; then
	echo "FAIL: rsyslog.out.log isn't expected to be generated!"
	exit 1
fi

exit_test
