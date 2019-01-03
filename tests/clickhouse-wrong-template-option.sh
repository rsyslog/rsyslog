#!/bin/bash
# add 2018-12-19 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

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
injectmsg
shutdown_when_empty
wait_shutdown

clickhouse-client --query="DROP TABLE rsyslog.template"
content_check "you have to specify the SQL or stdSQL option in your template!"

exit_test
