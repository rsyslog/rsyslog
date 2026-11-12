#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0
# Exercise the shared omclickhouse template requirement through YAML: SQL
# escaping must validate, while stdSQL and unescaped templates must reject
# the action. Explicit validation diagnostics are the oracle; no DB is used.
. ${srcdir:=.}/diag.sh init

for mode in sql stdsql none; do
	cat > "$RSYSLOG_DYNNAME.yaml" <<YAML
version: 2
modules:
  - load: "../plugins/omclickhouse/.libs/omclickhouse"
templates:
  - name: clickhouseFmt
    type: string
    string: "INSERT INTO rsyslog.SystemEvents (message) VALUES ('%msg%')"
YAML
	if [ "$mode" != none ]; then
		printf '    option.%s: "on"\n' "$mode" >> "$RSYSLOG_DYNNAME.yaml"
	fi
	cat >> "$RSYSLOG_DYNNAME.yaml" <<'YAML'
rulesets:
  - name: main
    script: |
      action(type="omclickhouse" template="clickhouseFmt"
             server="127.0.0.1" usehttps="off")
YAML
	../tools/rsyslogd -C -N1 -f "$RSYSLOG_DYNNAME.yaml" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.log" 2>&1
	if [ "$mode" = sql ]; then
		content_check 'End of config validation run' "$RSYSLOG_DYNNAME.log"
		check_not_present 'Action disabled. To use this action' "$RSYSLOG_DYNNAME.log"
	else
		content_check 'the SQL option (option.sql="on") in your template!' "$RSYSLOG_DYNNAME.log"
		check_not_present 'End of config validation run' "$RSYSLOG_DYNNAME.log"
	fi
done
exit_test
