#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# YAML companion of clickhouse-jsoneachrow.sh. The JSONEachRow action
# parameters (format, table, asyncinsert, compression) are set as native YAML
# action keys. A valid set must pass config validation, and a bad compression
# value or a table name that starts with a digit must fail it. Validation
# diagnostics are the oracle; no ClickHouse server is used.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse

run_yaml() {
    name="$1"
    table="$2"
    compression="$3"
    cat > "${RSYSLOG_DYNNAME}.${name}.yaml" <<YAMLEOF
templates:
  - name: chjson
    type: string
    string: "{\"message\":\"%msg:::json%\"}"

rulesets:
  - name: main
    filter: "*.*"
    actions:
      - type: omclickhouse
        server: "127.0.0.1"
        usehttps: "off"
        format: jsoneachrow
        table: "${table}"
        asyncinsert: "on"
        compression: "${compression}"
        template: chjson
YAMLEOF
    cat > "${RSYSLOG_DYNNAME}.${name}.conf" <<CONFEOF
module(load="../plugins/omclickhouse/.libs/omclickhouse")
include(file="${RSYSLOG_DYNNAME}.${name}.yaml")
CONFEOF
    ../tools/rsyslogd -C -N1 -f "${RSYSLOG_DYNNAME}.${name}.conf" -M"$RSYSLOG_MODDIR" \
        > "${RSYSLOG_DYNNAME}.${name}.log" 2>&1
}

if ! run_yaml valid "rsyslog.events" gzip; then
    printf 'FAIL: valid YAML JSONEachRow config was rejected\n'
    cat "${RSYSLOG_DYNNAME}.valid.log"
    error_exit 1
fi
content_check "End of config validation run" "${RSYSLOG_DYNNAME}.valid.log"

if run_yaml bad-compression "rsyslog.events" brotli; then
    printf 'FAIL: compression "brotli" was accepted\n'
    cat "${RSYSLOG_DYNNAME}.bad-compression.log"
    error_exit 1
fi
content_check "compression must be off or gzip" "${RSYSLOG_DYNNAME}.bad-compression.log"

if run_yaml digit-table "db.1events" gzip; then
    printf 'FAIL: table "db.1events" was accepted\n'
    cat "${RSYSLOG_DYNNAME}.digit-table.log"
    error_exit 1
fi
content_check "table must be" "${RSYSLOG_DYNNAME}.digit-table.log"

exit_test
