#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Jérémie Jourdin.
# YAML companion of clickhouse-http-reject-limit.sh. maxconsecutiverejects is
# set as a native YAML action key. A non-negative value must pass config
# validation and a negative one must fail it. Validation diagnostics are the
# oracle; no ClickHouse server is used.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin omclickhouse

run_yaml() {
    name="$1"
    limit="$2"
    cat > "${RSYSLOG_DYNNAME}.${name}.yaml" <<YAMLEOF
templates:
  - name: outfmt
    type: string
    string: "INSERT INTO rsyslog.limit (message) VALUES ('%msg%')"
    option.sql: "on"

rulesets:
  - name: main
    filter: "*.*"
    actions:
      - type: omclickhouse
        server: "127.0.0.1"
        usehttps: "off"
        maxconsecutiverejects: "${limit}"
        template: outfmt
YAMLEOF
    cat > "${RSYSLOG_DYNNAME}.${name}.conf" <<CONFEOF
module(load="../plugins/omclickhouse/.libs/omclickhouse")
include(file="${RSYSLOG_DYNNAME}.${name}.yaml")
CONFEOF
    ../tools/rsyslogd -C -N1 -f "${RSYSLOG_DYNNAME}.${name}.conf" -M"$RSYSLOG_MODDIR" \
        > "${RSYSLOG_DYNNAME}.${name}.log" 2>&1
}

if ! run_yaml valid 100; then
    printf 'FAIL: maxconsecutiverejects 100 was rejected\n'
    cat "${RSYSLOG_DYNNAME}.valid.log"
    error_exit 1
fi
content_check "End of config validation run" "${RSYSLOG_DYNNAME}.valid.log"

if run_yaml negative -1; then
    printf 'FAIL: maxconsecutiverejects -1 was accepted\n'
    cat "${RSYSLOG_DYNNAME}.negative.log"
    error_exit 1
fi
content_check "cannot be less than zero" "${RSYSLOG_DYNNAME}.negative.log"

exit_test
