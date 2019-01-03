#!/bin/bash
# add 2018-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omclickhouse/.libs/omclickhouse")
input(type="imtcp" port="'$TCPFLOOD_PORT'")

template(name="outfmt" option.stdsql="on" type="string" string="SELECT * FROM rsyslog.select")

:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443"
					bulkmode="off" user="default" pwd=""
					template="outfmt")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.select ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: msgnum:00000001\""
shutdown_when_empty
wait_shutdown

content_check "omclickhouse: Message is no Insert query: Message suspended: SELECT * FROM rsyslog.select"

clickhouse-client --query="DROP TABLE rsyslog.select"
exit_test
