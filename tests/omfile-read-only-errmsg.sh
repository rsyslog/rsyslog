#!/bin/bash
# addd 2017-03-01 by RGerhards, released under ASL 2.0
# Verifies that omfile logs an open error for a read-only output file. The
# oracle requires the current user and filesystem to reject appends to a 0400
# file; if the platform still permits that write, the diagnostic cannot be
# triggered and the test is skipped.

. ${srcdir:=.}/diag.sh init
skip_ASAN "omfile read-only error message format differs under ASan"
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
:msg, contains, "msgnum:" {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG2_OUT_LOG`)
}

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`)
'
touch ${RSYSLOG2_OUT_LOG}
chmod 0400 ${RSYSLOG2_OUT_LOG}
if (: >> "${RSYSLOG2_OUT_LOG}") 2>/dev/null; then
	echo "SKIP: environment can append to a 0400 file; read-only omfile error path is not testable"
	skip_test
fi
ls -l rsyslog.ou*
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

grep "${RSYSLOG2_OUT_LOG}.* open error"  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected error message not found.  $RSYSLOG_OUT_LOG is:"
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi

exit_test
