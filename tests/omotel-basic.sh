#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotel-basic.sh -- basic functionality test for omotel module
##
## Starts OTEL Collector, sends messages via omotel, and verifies
## messages are received and stored correctly in OTLP JSON format.

. ${srcdir:=.}/diag.sh init

# Check if omotel module is available
# NOTE: The module must be enabled during configure with --enable-omotel
# If the module fails to load with error -1001 (RS_RET_MISSING_INTERFACE),
# the module was not built correctly and needs to be rebuilt with --enable-omotel
require_plugin omotel

export NUMMESSAGES=1000
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

generate_conf
add_conf '
template(name="otlpBody" type="string" string="msgnum:%msg:F,58:2%")

module(load="../plugins/omotel/.libs/omotel")

if $msg contains "msgnum:" then
	action(
		name="omotel-http"
		type="omotel"
		template="otlpBody"
		endpoint="http://127.0.0.1:'$otel_port'"
		path="/v1/logs"
		batch.max_items="100"
		batch.timeout.ms="1000"
	)
'

startup
injectmsg
shutdown_when_empty
wait_shutdown

# Stop OTEL Collector to ensure file exporter flushes data
# The file exporter may buffer data until collector shuts down
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
    sys.stderr.write(f"omotel-basic: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not records:
    sys.stderr.write("omotel-basic: OTLP output did not contain any logRecords\n")
    sys.exit(1)

def has_hostname(attrs):
    for entry in attrs:
        if entry.get("key") == "log.syslog.hostname":
            val = entry.get("value", {}).get("stringValue", "")
            if val:
                return True
    return False

for idx, record in enumerate(records):
    if record.get("severityNumber", 0) == 0:
        sys.stderr.write(f"omotel-basic: record {idx} missing severityNumber\n")
        sys.exit(1)
    if not has_hostname(record.get("attributes", [])):
        sys.stderr.write(f"omotel-basic: record {idx} missing log.syslog.hostname attribute\n")
        sys.exit(1)
PY

seq_check
exit_test
