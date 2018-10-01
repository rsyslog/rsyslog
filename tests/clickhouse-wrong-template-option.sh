#!/bin/bash
# add 2018-12-19 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omclickhouse/.libs/omclickhouse")
input(type="imtcp" port="'$TCPFLOOD_PORT'")


template(name="outfmt" type="string" string="INSERT INTO rsyslog.template (id, severity, facility, timestamp, ipaddress, tag, message) VALUES (%msg:F,58:2%, %syslogseverity%, %syslogfacility%, '
add_conf "'%timereported:::date-unixtimestamp%', '%fromhost-ip%', '%syslogtag%', '%msg%')"
add_conf '")


:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost"
					usehttps="off" bulkmode="off"
					user="default" pwd="" template="outfmt")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.template ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"

startup
tcpflood -m1 -M "\"<130>Mar 10 01:00:00 172.20.245.8 tag: msgnum:00000001\""
shutdown_when_empty
wait_shutdown

content_check "you have to specify the SQL or stdSQL option in your template!"

clickhouse-client --query="DROP TABLE rsyslog.template"
exit_test
