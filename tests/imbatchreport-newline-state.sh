#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Batch-report space deduplication must not remove an earlier encoded newline
# when trailing spaces are followed by blank lines. Compare exact message bytes
# for dedup on/off, ordinary spaces, single and multiple newlines. Each report
# ends in text (no final LF) to isolate this from EOF trimming/structured data.
# Use a fresh single-input daemon for each dedup mode; wait for five additional
# records before each shutdown. Sort only the records because glob
# traversal order is unspecified, retaining every byte within each message.
. ${srcdir:=.}/diag.sh init
require_plugin imbatchreport ../contrib/imbatchreport

records=0
for mode in on off; do
if [ "${IMBATCHREPORT_TEST_YAML:-0}" = 1 ]; then
 generate_conf --yaml-only
 add_yaml_conf '
modules:
  - load: "../contrib/imbatchreport/.libs/imbatchreport"
    pollinginterval: "1"
templates:
  - name: reportfmt
    type: string
    string: "%syslogtag%|%msg%\n"
inputs:
  - type: imbatchreport
    reports: "./'$RSYSLOG_DYNNAME'.'$mode'.*.done"
    tag: "'$mode'"
    deduplicatespace: "'$mode'"
    delete: ".done$ .rejected"
    ruleset: reports
rulesets:
  - name: reports
    script: |
      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="reportfmt")
'
else
 generate_conf
 add_conf '
module(load="../contrib/imbatchreport/.libs/imbatchreport" pollinginterval="1")
template(name="reportfmt" type="string" string="%syslogtag%|%msg%\n")
input(type="imbatchreport" reports="./'$RSYSLOG_DYNNAME'.'$mode'.*.done" tag="'$mode'"
      deduplicatespace="'$mode'" delete=".done$ .rejected" ruleset="reports")
ruleset(name="reports") {
 action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="reportfmt")
}
'
fi

 printf 'blank\n\nbeta' > "$RSYSLOG_DYNNAME.$mode.blank.done"
 printf 'trailing  \n\nbeta' > "$RSYSLOG_DYNNAME.$mode.trailing.done"
 printf 'multiple \n\n\nbeta' > "$RSYSLOG_DYNNAME.$mode.multiple.done"
 printf 'single \nbeta' > "$RSYSLOG_DYNNAME.$mode.single.done"
 printf 'ordinary  words\nbeta' > "$RSYSLOG_DYNNAME.$mode.ordinary.done"

# Preserve each generated frontend config for baseline failure evidence.
cp "$TESTCONF_NM.$TESTCONF_EXT" "$RSYSLOG_DYNNAME.$mode.config.$TESTCONF_EXT"
startup
records=$((records + 5))
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" "$records"
shutdown_when_empty
wait_shutdown
done

cat > "$RSYSLOG_DYNNAME.expected-unsorted" <<'EXPECTED'
on|blank\n\nbeta
on|trailing\n\nbeta
on|multiple\n\n\nbeta
on|single\nbeta
on|ordinary words\nbeta
off|blank\n\nbeta
off|trailing  \n\nbeta
off|multiple \n\n\nbeta
off|single \nbeta
off|ordinary  words\nbeta
EXPECTED
LC_ALL=C sort "$RSYSLOG_DYNNAME.expected-unsorted" > "$RSYSLOG_DYNNAME.expected"
LC_ALL=C sort "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.actual"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_DYNNAME.actual"
for report in "$RSYSLOG_DYNNAME".*.done; do
 [ ! -e "$report" ] || error_exit 1 "report was not consumed: $report"
done
exit_test
