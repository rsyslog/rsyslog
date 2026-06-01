#!/usr/bin/env bash
# Verify that omclickhouse accepts a custom INSERT template when the SQL
# escaping requirement is expressed with the template-level option.stdsql
# parameter. This covers the config-validation path from issue #6297 without
# requiring a live ClickHouse service.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="CustomClickHouseFmt"
         type="string"
         option.stdsql="on"
         string="INSERT INTO rsyslog.SystemEvents (severity, facility, timestamp, hostname, fromhost_ip, tag, message) VALUES (%syslogseverity%, %syslogfacility%, '\''%timereported:::date-unixtimestamp%'\'', '\''%hostname%'\'', '\''%fromhost-ip%'\'', '\''%syslogtag:1:32%'\'', '\''%msg%'\'')")

action(type="omclickhouse"
       server="127.0.0.1"
       user="writer"
       pwd="secret"
       usehttps="off"
       bulkmode="on"
       template="CustomClickHouseFmt")
'

../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.log" 2>&1
content_check "End of config validation run" "$RSYSLOG_DYNNAME.log"
check_not_present "Action disabled. To use this action" "$RSYSLOG_DYNNAME.log"

exit_test
