#!/bin/bash
# add 2018-12-31 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omclickhouse/.libs/omclickhouse")
input(type="imtcp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" option.stdsql="on" type="string" string="INSERT INTO rsyslog.retryerror (id, severity, message) VALUES (%msg:F,58:2%, %syslogseverity%, '
add_conf "'%msg%')"
add_conf '")

:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost"
					bulkmode="off" user="default" pwd=""
					template="outfmt")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
tcpflood -m1
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: checkConn failed."

exit_test
