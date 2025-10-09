#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-http-batch.sh -- smoke-test OTLP batching and retries
##
## Starts the omhttp test server, emits four messages through omotlp with
## batching and gzip enabled, and verifies the collector captured two payloads
## with the expected retry behaviour and record order.

. ${srcdir:=.}/diag.sh init

# Check if omotlp module is available
require_plugin omotlp

omhttp_start_server 0 --decompress --fail-every 2 --fail-with 503

generate_conf
add_conf '
module(load="../plugins/omotlp/.libs/omotlp")
template(name="otlpBody" type="string" string="%msg%")

# Process all messages through omotlp
action(
  name="omotlp-http"
  type="omotlp"
  template="otlpBody"
  endpoint="http://127.0.0.1:'$omhttp_server_lstnport'"
  path="/v1/logs"
  batch.max_items="2"
  batch.timeout.ms="1000"
  compression="gzip"
  retry.initial.ms="10"
  retry.max.ms="100"
  retry.max_retries="3"
  headers='{ "X-Test-Header": "omotlp" }'
)
'

startup
injectmsg_literal 'msg 1
msg 2
msg 3
msg 4'

# Wait for batches to flush (batch.timeout.ms="1000" + retry time)
# With batch.max_items="2", we expect 2 batches:
# - Batch 1 (msg 1-2): should succeed immediately
# - Batch 2 (msg 3-4): will fail with 503, then retry up to 3 times
# Worst case retry time: 3 retries with exponential backoff up to 100ms = ~700ms
# Add buffer for batch timeout (1000ms) + retries + network delays
if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
	$TESTTOOL_DIR/msleep 3000
else
	sleep 3
fi

shutdown_when_empty
wait_shutdown

# Give a bit more time for any final retries to complete
if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
	$TESTTOOL_DIR/msleep 1000
else
	sleep 1
fi

attempt=0
ret=1
while [ $attempt -lt 30 ]; do
python3 - "$omhttp_server_lstnport" <<'PY'
import json
import sys
import urllib.request

port = int(sys.argv[1])
try:
    with urllib.request.urlopen(f"http://127.0.0.1:{port}/v1/logs") as resp:
        payloads = json.load(resp)
except Exception as e:
    sys.stderr.write(f"failed to fetch payloads: {e}\n")
    sys.exit(1)

if len(payloads) != 2:
    sys.stderr.write(f"expected 2 payloads, got {len(payloads)}\n")
    if len(payloads) > 0:
        sys.stderr.write(f"first payload preview: {str(payloads[0])[:200]}...\n")
    sys.exit(1)

records = []
batch_sizes = []
for payload in payloads:
    try:
        document = json.loads(payload)
        logs = document["resourceLogs"][0]["scopeLogs"][0]["logRecords"]
        batch_sizes.append(len(logs))
        for entry in logs:
            body = entry.get("body", {}).get("stringValue")
            if body:
                records.append(body)
    except (KeyError, json.JSONDecodeError) as e:
        sys.stderr.write(f"failed to parse payload: {e}\n")
        sys.stderr.write(f"payload: {payload[:200]}...\n")
        sys.exit(1)

expected = [f"msg {i}" for i in range(1, 5)]
if batch_sizes != [2, 2]:
    sys.stderr.write(f"unexpected batch sizes {batch_sizes}, expected [2, 2]\n")
    sys.exit(1)

if records != expected:
    sys.stderr.write(f"unexpected bodies {records}, expected {expected}\n")
    sys.exit(1)
PY
    ret=$?
    if [ $ret -eq 0 ]; then
        break
    fi
    attempt=$((attempt + 1))
    if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
        $TESTTOOL_DIR/msleep 200
    else
        sleep 0.2
    fi
done

omhttp_stop_server

exit_test $ret
