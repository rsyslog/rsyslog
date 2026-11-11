#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that the default omclickhouse template keeps a backslash-plus-quote
# payload inside the message literal instead of allowing a second VALUES row.
# Exactly one row and byte-for-byte hex equality prove both containment and
# preservation; the shared ClickHouse chain provides HTTPS on port 8443.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")

:syslogtag, contains, "tag" action(type="omclickhouse" server="localhost" port="8443"
					bulkmode="off" user="default" pwd="")
'

clickhouse-client --query="CREATE TABLE IF NOT EXISTS rsyslog.SystemEvents ( severity Int8, facility Int8, timestamp DateTime, hostname String, tag String, message String ) ENGINE = MergeTree() PARTITION BY severity order by tuple()"

payload="\\'),(0,0,now(),version(),version(),(SELECT name FROM system.tables ORDER BY name LIMIT 1))-- "
payload_hex=$(printf '%s' "$payload" | od -An -v -tx1 | tr -d ' \n' | tr 'a-f' 'A-F')

startup
injectmsg_literal "<130>Mar 10 01:00:00 192.0.2.8 tag:${payload}"
shutdown_when_empty
wait_shutdown

actual=$(clickhouse-client --query="SELECT count() FROM rsyslog.SystemEvents")
[ "$actual" = "1" ] || error_exit 1 "Expected exactly one row, got $actual"
actual=$(clickhouse-client --query="SELECT hex(message) FROM rsyslog.SystemEvents")
[ "$actual" = "$payload_hex" ] || error_exit 1 "Message bytes changed: $actual"

clickhouse-client --query="DROP TABLE rsyslog.SystemEvents"
exit_test
