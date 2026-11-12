#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that omclickhouse accepts a custom INSERT template only when it uses
# the backslash-aware SQL escaping mode.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

template(name="CustomClickHouseFmt"
         type="string"
         option.sql="on"
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
