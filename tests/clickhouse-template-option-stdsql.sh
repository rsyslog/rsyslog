#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that omclickhouse rejects standard SQL escaping during validation.
# The diagnostic and absence of successful validation prove the unsafe action
# is rejected without requiring a ClickHouse service.
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

../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.log" 2>&1 || true
content_check 'the SQL option (option.sql="on") in your template!' "$RSYSLOG_DYNNAME.log"
check_not_present "End of config validation run" "$RSYSLOG_DYNNAME.log"

exit_test
