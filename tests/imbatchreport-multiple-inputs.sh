#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Each configured input must remain reachable: three non-overlapping globs
# contain three reports each. The nine exact tag/content records and deletion
# of all nine source files prove that every instance was polled. Files exist
# before startup; the output-count wait proves delivery before synchronized
# shutdown. Sorting removes only unspecified glob/instance traversal order.
. ${srcdir:=.}/diag.sh init
require_plugin imbatchreport ../contrib/imbatchreport

if [ "${IMBATCHREPORT_TEST_YAML:-0}" = 1 ]; then
 generate_conf --yaml-only
 config='modules:
  - load: "../contrib/imbatchreport/.libs/imbatchreport"
    pollinginterval: "1"
templates:
  - name: reportfmt
    type: string
    string: "%syslogtag%|%msg%\n"
inputs:'
 for label in first second third; do
  config+='
  - type: imbatchreport
    reports: "./'"$RSYSLOG_DYNNAME.$label"'.*.done"
    tag: "'"$label"'"
    delete: ".done$ .rejected"
    ruleset: reports'
 done
 config+='
rulesets:
  - name: reports
    script: |
      action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="reportfmt")'
 add_yaml_conf "$config"
else
 generate_conf
 add_conf '
module(load="../contrib/imbatchreport/.libs/imbatchreport" pollinginterval="1")
template(name="reportfmt" type="string" string="%syslogtag%|%msg%\n")
ruleset(name="reports") {
 action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="reportfmt")
}'
 for label in first second third; do
  add_conf 'input(type="imbatchreport" reports="./'"$RSYSLOG_DYNNAME.$label"'.*.done"
 tag="'"$label"'" delete=".done$ .rejected" ruleset="reports")'
 done
fi

: > "$RSYSLOG_DYNNAME.expected-unsorted"
for label in first second third; do
 for index in 1 2 3; do
  printf '%s' "$label-$index" > "$RSYSLOG_DYNNAME.$label.$index.done"
  printf '%s|%s\n' "$label" "$label-$index" >> "$RSYSLOG_DYNNAME.expected-unsorted"
 done
done
rsyslogd_config_check || error_exit 1 "generated multi-input configuration is invalid"
startup
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 9
shutdown_when_empty
wait_shutdown
LC_ALL=C sort "$RSYSLOG_DYNNAME.expected-unsorted" > "$RSYSLOG_DYNNAME.expected"
LC_ALL=C sort "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.actual"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_DYNNAME.actual"
for report in "$RSYSLOG_DYNNAME".*.done; do
 [ ! -e "$report" ] || error_exit 1 "report was not consumed: $report"
done
exit_test
