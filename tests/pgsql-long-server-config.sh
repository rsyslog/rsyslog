#!/usr/bin/env bash
# Verify that the modern ompgsql action parameter parser accepts DNS names
# longer than the historical MAXHOSTNAMELEN-sized storage. This covers issue
# #4698 with config validation only; no PostgreSQL server is contacted.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/ompgsql/.libs/ompgsql")

template(name="PgSQLLongServerFmt" type="string" option.sql="on"
         string="insert into SystemEvents (Message) values ('\''%msg%'\'')")

action(type="ompgsql"
       server="very-long-postgresql-endpoint-name-for-rsyslog-issue-4698.cluster-example.internal"
       db="syslog"
       user="rsyslog"
       pass="secret"
       template="PgSQLLongServerFmt")
'

../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.log" 2>&1
content_check "End of config validation run" "$RSYSLOG_DYNNAME.log"
check_not_present "srv parameter longer than supported maximum" "$RSYSLOG_DYNNAME.log"

exit_test
