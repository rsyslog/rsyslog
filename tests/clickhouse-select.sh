#!/bin/bash
# add 2018-12-07 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=1
generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.stdsql="on" type="string" string="SELECT * FROM rsyslog.select")

:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443"
					bulkmode="off" user="default" pwd=""
					template="outfmt")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.select ( id Int32, severity Int8, facility Int8, timestamp DateTime, ipaddress String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity Order By id"

startup
injectmsg
shutdown_when_empty
wait_shutdown

clickhouse-client --query="DROP TABLE rsyslog.select"
content_check "omclickhouse: Message is no Insert query: Message suspended: SELECT * FROM rsyslog.select"

exit_test
