#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-trace-correlation.sh -- test trace correlation support for omotlp module
##
## Tests that trace_id, span_id, and trace_flags are extracted from message
## properties and included in the OTLP export. Uses mmjsonparse to extract
## trace properties from JSON messages.

. ${srcdir:=.}/diag.sh init

# Check if omotlp module is available
require_plugin omotlp
require_plugin mmjsonparse

export NUMMESSAGES=10
export EXTRA_EXIT=otel
export SEQ_CHECK_OPTIONS=-d

# Download and prepare OTEL Collector
download_otel_collector
prepare_otel_collector
start_otel_collector

# Read the port from the port file
if [ ! -f ${RSYSLOG_DYNNAME}.otel_port.file ]; then
	echo "ERROR: OTEL Collector port file not found: ${RSYSLOG_DYNNAME}.otel_port.file"
	error_exit 1
fi
otel_port=$(cat ${RSYSLOG_DYNNAME}.otel_port.file)
if [ -z "$otel_port" ]; then
	echo "ERROR: OTEL Collector port is empty"
	error_exit 1
fi
echo "Using OTEL Collector port: $otel_port"

# Create input file with syslog messages containing JSON with trace context
cat > ${RSYSLOG_DYNNAME}.input << 'INPUTFILE'
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000001"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000002"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000003"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000004"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000005"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000006"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000007"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000008"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000009"}
<14>1 2024-01-01T12:00:00.000Z testhost testapp 12345 - [test@123] {"trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","trace_flags":"01","message":"test message 00000010"}
INPUTFILE

generate_conf
add_conf '
template(name="otlpBody" type="string" string="%msg%")

module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/omotlp/.libs/omotlp")

# Parse JSON to extract trace properties
action(type="mmjsonparse" mode="find-json")

# Export with trace correlation
action(
	name="omotlp-http"
	type="omotlp"
	template="otlpBody"
	endpoint="http://127.0.0.1:'$otel_port'"
	path="/v1/logs"
	trace_id.property="trace_id"
	span_id.property="span_id"
	trace_flags.property="trace_flags"
	batch.max_items="100"
	batch.timeout.ms="1000"
)
'

startup
injectmsg_file ${RSYSLOG_DYNNAME}.input
shutdown_when_empty
wait_shutdown

# Stop OTEL Collector to ensure file exporter flushes data
stop_otel_collector

# Give OTEL Collector a moment to flush the output file after shutdown
if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
	$TESTTOOL_DIR/msleep 1000
else
	sleep 1
fi

# Extract data from OTEL Collector output
otel_collector_get_data

python3 - "$RSYSLOG_DYNNAME.otel-output.json" <<'PY'
import json
import sys

path = sys.argv[1]
try:
    # OTEL Collector file exporter writes newline-delimited JSON (one JSON object per line)
    records = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            payload = json.loads(line)
            if "resourceLogs" in payload:
                for resource_log in payload["resourceLogs"]:
                    if "scopeLogs" in resource_log:
                        for scope_log in resource_log["scopeLogs"]:
                            if "logRecords" in scope_log:
                                records.extend(scope_log["logRecords"])
except Exception as exc:
    sys.stderr.write(f"omotlp-trace-correlation: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not records:
    sys.stderr.write("omotlp-trace-correlation: OTLP output did not contain any logRecords\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-trace-correlation: found {len(records)} log records in OTLP output\n")

# Expected trace values
expected_trace_id = "4bf92f3577b34da6a3ce929d0e0e4736"
expected_span_id = "00f067aa0ba902b7"
expected_trace_flags = 1

sys.stdout.write(f"omotlp-trace-correlation: expected traceId='{expected_trace_id}', spanId='{expected_span_id}', flags={expected_trace_flags}\n")

# Verify trace correlation fields
for idx, record in enumerate(records):
    trace_id = record.get("traceId")
    span_id = record.get("spanId")
    trace_flags = record.get("flags")
    
    sys.stdout.write(f"omotlp-trace-correlation: record {idx}: traceId={trace_id}, spanId={span_id}, flags={trace_flags}\n")
    
    if trace_id is None:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} missing traceId\n")
        sys.exit(1)
    if trace_id != expected_trace_id:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} traceId mismatch: expected '{expected_trace_id}', got '{trace_id}'\n")
        sys.exit(1)
    
    if span_id is None:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} missing spanId\n")
        sys.exit(1)
    if span_id != expected_span_id:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} spanId mismatch: expected '{expected_span_id}', got '{span_id}'\n")
        sys.exit(1)
    
    if trace_flags is None:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} missing flags\n")
        sys.exit(1)
    if trace_flags != expected_trace_flags:
        sys.stderr.write(f"omotlp-trace-correlation: record {idx} flags mismatch: expected {expected_trace_flags}, got {trace_flags}\n")
        sys.exit(1)

sys.stdout.write(f"omotlp-trace-correlation: verified {len(records)} records with trace correlation\n")
PY

exit_test

