#!/bin/bash
# add 2018-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omclickhouse/.libs/omclickhouse")
input(type="imtcp" port="'$TCPFLOOD_PORT'")


template(name="outfmt" option.stdsql="on" type="string" string="INSERT INTO rsyslog.basic (id, severity, facility, timestamp, ipaddress, tag, message) VALUES (%msg:F,58:2%, %syslogseverity%, %syslogfacility%, '
add_conf "'%timereported:::date-unixtimestamp%', '%fromhost-ip%', '%syslogtag%', '%msg%')"
add_conf '")


:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443" bulkmode="off"
					user="default" pwd="" template="outfmt")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.basic ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"

startup
tcpflood -m1 -M "\"<130>Mar 10 01:00:00 172.20.245.8 tag: msgnum:00000001\""
shutdown_when_empty
wait_shutdown
clickhouse-client --query="SELECT id, severity, facility, ipaddress, tag, message FROM rsyslog.basic" > $RSYSLOG_OUT_LOG

export EXPECTED='1	2	16	127.0.0.1	tag:	 msgnum:00000001'
cmp_exact $RSYSLOG_OUT_LOG

clickhouse-client --query="DROP TABLE rsyslog.basic"
exit_test
