#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0
# SQL mode does not escape plugin/subtree output, even when declared on the template.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/omclickhouse/.libs/omclickhouse")
template(name="UnsafeClickHouseFmt" type="subtree" subtree="$!query" option.sql="on")
action(type="omclickhouse" server="127.0.0.1" usehttps="off"
       template="UnsafeClickHouseFmt")
'

if ../tools/rsyslogd -C -N1 -f "${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.log" 2>&1; then
    echo "FAIL: expected subtree template rejection"
    cat "$RSYSLOG_DYNNAME.log"
    error_exit 1
fi
content_check "Backslash-aware SQL escaping requires a string or list template" "$RSYSLOG_DYNNAME.log"
check_not_present "End of config validation run" "$RSYSLOG_DYNNAME.log"

exit_test
