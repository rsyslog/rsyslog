#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-batch.sh -- batching test for omotlp module
##
## Tests that omotlp correctly batches multiple log records into
## single ExportLogsServiceRequest payloads.

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=10000
export EXTRA_EXIT=otel

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

module(load="../plugins/omotlp/.libs/omotlp")

if $msg contains "msgnum:" then
	action(
		name="omotlp-http"
		type="omotlp"
		template="otlpBody"
		endpoint="http://127.0.0.1:'$otel_port'"
		path="/v1/logs"
		batch.max_items="3"
		batch.timeout.ms="60000"
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

seq_check
exit_test
