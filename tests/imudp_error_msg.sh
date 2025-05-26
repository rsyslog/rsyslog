#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="514" address="128.98.1.12")
input(type="imudp" port="514" address="128.98.1.12")

template(name="outfmt" type="string" string="-%msg%-\n")
:syslogtag, contains, "tag" action(type="omfile" template="outfmt"
			         file=`echo $RSYSLOG2_OUT_LOG`)

action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
'
startup
shutdown_when_empty
wait_shutdown

grep "imudp: Could not create udp listener, ignoring port 514 bind-address 128.98.1.12."  $RSYSLOG_OUT_LOG > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found.  $RSYSLOG_OUT_LOG is:"
        cat $RSYSLOG_OUT_LOG
        error_exit 1
fi

exit_test
