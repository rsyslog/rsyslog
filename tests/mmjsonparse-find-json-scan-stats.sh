#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A completed find-json search must not count as scan.truncated merely because
# its last byte is an unsuccessful '{' candidate. Only the real byte cutoff
# counts. Short/exact-limit final braces, ordinary misses, and a valid object
# also verify unchanged parse results and exact message lengths.
# One worker/action and cumulative impstats counters make the seven-message
# oracle exact. Wait for output and a post-stimulus stats snapshot, not a sleep;
# the normal bounded helpers allow the periodic stats writer to catch up.
. "${srcdir:=.}/diag.sh" init
require_plugin imtcp
require_plugin mmjsonparse
require_plugin impstats
export NUMMESSAGES=7

"$PYTHON" - "$RSYSLOG_DYNNAME" <<'PY'
import sys
base = sys.argv[1]
payloads = ['plain', 'prefix{', '{{', 'x' * 62 + '{', 'x' * 62 + '{' + 'x' * 17, '{"value":1}', 'prefix{x']
with open(base + '.input', 'w') as stream:
    stream.write(''.join(
        '<167>Jan 16 16:57:54 host.example.net scan: ' + value + '\n' for value in payloads))
with open(base + '.expected', 'w') as stream:
    stream.write(''.join(
        ' ' + value + '|' + ('OK' if value == '{"value":1}' else 'FAIL') + '|' + str(len(value) + 1) + '\n'
        for value in payloads))
PY

if [ "${MMJSONPARSE_STATS_YAML:-0}" = 1 ]; then
 generate_conf --yaml-only
 add_yaml_conf '
main_queue:
  queue.workerThreads: "1"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
  - load: "../plugins/mmjsonparse/.libs/mmjsonparse"
  - load: "../plugins/impstats/.libs/impstats"
    interval: "1"
    resetCounters: "off"
    log.file: "'$RSYSLOG_DYNNAME'.stats"
    log.syslog: "off"
    format: "json"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"
    ruleset: scan
templates:
  - name: outfmt
    type: string
    string: "%msg%|%parsesuccess%|%$.msglen%\n"
rulesets:
  - name: scan
    script: |
      action(type="mmjsonparse" mode="find-json" max_scan_bytes="64")
      set $.msglen = strlen($msg);
      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
else
 generate_conf
 add_conf '
main_queue(queue.workerThreads="1")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/impstats/.libs/impstats" interval="1" resetCounters="off"
       log.file="'$RSYSLOG_DYNNAME'.stats" log.syslog="off" format="json")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="scan")
template(name="outfmt" type="string" string="%msg%|%parsesuccess%|%$.msglen%\n")
ruleset(name="scan") {
    action(type="mmjsonparse" mode="find-json" max_scan_bytes="64")
    set $.msglen = strlen($msg);
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
fi
if [ "${MMJSONPARSE_STATS_YAML:-0}" = 1 ]; then
 cp "${TESTCONF_NM}.yaml" "$RSYSLOG_DYNNAME.config"
else
 cp "${TESTCONF_NM}.conf" "$RSYSLOG_DYNNAME.config"
fi
startup
tcpflood -m1 -I "$RSYSLOG_DYNNAME.input"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 7
wait_content '"scan.attempted": *7' "$RSYSLOG_DYNNAME.stats"
shutdown_when_empty
wait_shutdown
cmp "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG" || error_exit 1
"$PYTHON" - "$RSYSLOG_DYNNAME.stats" <<'PY'
import json
import sys
# The direct impstats log prefixes each JSON object with a local timestamp.
rows = [json.loads(line[line.index('{'):]) for line in open(sys.argv[1]) if '{' in line]
rows = [row for row in rows if row.get('origin') == 'mmjsonparse' and row.get('scan.attempted') == 7]
assert rows, 'no completed mmjsonparse stats snapshot'
for row in rows:
    actual = {key: row[key] for key in ('scan.attempted', 'scan.found', 'scan.truncated')}
    expected = {'scan.attempted': 7, 'scan.found': 1, 'scan.truncated': 1}
    print('scan counters:', actual)
    assert actual == expected, (actual, expected)
PY
[ "$?" -eq 0 ] || error_exit 1
exit_test
