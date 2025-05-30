#!/bin/bash
# add 2018-05-17 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
export IMFILECHECKTIMEOUT="60"

generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" freshStartTail="on" Tag="pro"
	File="'$RSYSLOG_DYNNAME'.input.*")

template(name="outfmt" type="string" string="%msg%\n")

:syslogtag, contains, "pro" action(type="omfile" File=`echo $RSYSLOG_OUT_LOG`
	template="outfmt")
'
startup

echo '{ "id": "jinqiao1"}' > $RSYSLOG_DYNNAME.input.a
content_check_with_count '{ "id": "jinqiao1"}' 1 $IMFILECHECKTIMEOUT

echo '{ "id": "jinqiao2"}' >> $RSYSLOG_DYNNAME.input.a
content_check_with_count '{ "id": "jinqiao2"}' 1 $IMFILECHECKTIMEOUT

shutdown_when_empty
wait_shutdown
exit_test
