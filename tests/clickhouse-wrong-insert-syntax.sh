#!/bin/bash
# add 2018-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
echo looks like clickhouse does no longer generate exceptions on error - skip until investigated
exit 77
export NUMMESSAGES=1
generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.stdsql="on" type="string" string="INSERT INTO rsyslog.wrongInsertSyntax (id, severity, facility, timestamp, ipaddress, tag, message) VALUES (%msg:F,58:2%, %syslogseverity%, %syslogfacility%, '
add_conf "'%timereported:::date-unixtimestamp%', '%fromhost-ip%', '%syslogtag%', '%msg%')"
add_conf '")


:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443"
					user="default" pwd="" template="outfmt"
					bulkmode="off" errorfile="'$RSYSLOG_OUT_LOG'")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.wrongInsertSyntax ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"

startup
injectmsg
shutdown_when_empty
wait_shutdown

clickhouse-client --query="DROP TABLE rsyslog.wrongInsertSyntax"
content_check "DB::Exception: Syntax error"
exit_test
