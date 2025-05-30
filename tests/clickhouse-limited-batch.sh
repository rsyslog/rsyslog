#!/bin/bash
# add 2018-12-20 by Pascal Withopf, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100000

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="outfmt" option.stdsql="on" type="string" string="INSERT INTO rsyslog.limited (id, ipaddress, message) VALUES (%msg:F,58:2%, '
add_conf "'%fromhost-ip%', '%msg:F,58:2%')"
add_conf '")


:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443"
					user="default" pwd="" template="outfmt"
					maxbytes="1k")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.limited ( id Int32, ipaddress String, message String ) ENGINE = MergeTree() PARTITION BY ipaddress Order By id"

startup
injectmsg
shutdown_when_empty
wait_shutdown
clickhouse-client --query="SELECT message FROM rsyslog.limited ORDER BY id" > $RSYSLOG_OUT_LOG

clickhouse-client --query="DROP TABLE rsyslog.limited"
seq_check  0 $(( NUMMESSAGES - 1 ))

exit_test
