#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
# Verify that a Bulk API response with errors=true is inspected even when
# retryfailures and errorfile are unset. Invalid integer documents must raise
# omelasticsearch's failed.es counter; a successful-response fast path would
# leave it at zero. Queue and stats-flush helpers provide the completion oracle.
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=100
ensure_elasticsearch_ready

init_elasticsearch
curl -s -H 'Content-Type: application/json' -XPUT "localhost:${ES_PORT}/rsyslog_testbench/" -d '{
  "mappings": {
    "properties": {
      "msgnum": {
        "type": "integer"
      }
    }
  }
}' >/dev/null

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"x%msg:F,58:2%\"}")

module(load="../plugins/impstats/.libs/impstats" interval="1"
	   log.file="'"$RSYSLOG_DYNNAME"'.spool/omelasticsearch-stats.log"
	   log.syslog="off" format="cee" bracketing="on")
module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       serverport="'"$ES_PORT"'"
	       template="tpl"
	       searchIndex="rsyslog_testbench"
	       bulkmode="on")
'

startup
injectmsg 0 "$NUMMESSAGES"
wait_queueempty
wait_for_stats_flush "${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log"
shutdown_when_empty
wait_shutdown

$PYTHON <"${RSYSLOG_DYNNAME}.spool/omelasticsearch-stats.log" -c '
import json
import sys

expected_submitted = int(sys.argv[1])
stats = None
for line in sys.stdin:
    start = line.find("{")
    if start >= 0:
        record = json.loads(line[start:])
        if record.get("origin") == "omelasticsearch":
            stats = record

if stats is None:
    raise SystemExit("FAIL: omelasticsearch statistics were not emitted")
if stats.get("submitted") != expected_submitted:
    raise SystemExit("FAIL: expected {} submitted messages, got {}".format(
        expected_submitted, stats.get("submitted")))
if stats.get("failed.es", 0) < 1:
    raise SystemExit("FAIL: expected failed.es to record the bulk item errors")
' "$NUMMESSAGES" || error_exit 1

exit_test
