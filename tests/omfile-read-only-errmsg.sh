#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG2_OUT_LOG`)
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
touch rsyslog2.out.log
chmod 0400 rsyslog2.out.log
ls -l rsyslog.ou*
startup
$srcdir/diag.sh injectmsg 0 1
shutdown_when_empty
wait_shutdown

grep "rsyslog2.out.log.* open error"  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found.  $RSYSLOG_OUT_LOG is:"
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi

exit_test
