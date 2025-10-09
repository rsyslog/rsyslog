#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-proxy.sh -- proxy support test for omotlp module
##
## Tests that omotlp correctly uses HTTP proxy to forward requests to
## OTEL Collector, with and without proxy authentication.

. ${srcdir:=.}/diag.sh init

# Check if omotlp module is available
require_plugin omotlp

export NUMMESSAGES=100
export EXTRA_EXIT="otel proxy"
export SEQ_CHECK_OPTIONS=-d

# Check for Python 3 (required for proxy server)
if ! command -v python3 &> /dev/null; then
	echo "ERROR: python3 not found - required for proxy server"
	error_exit 77
fi

# Download and prepare OTEL Collector
download_otel_collector
prepare_otel_collector
start_otel_collector

# Read the OTEL Collector port
if [ ! -f ${RSYSLOG_DYNNAME}.otel_port.file ]; then
	echo "ERROR: OTEL Collector port file not found"
	error_exit 1
fi
otel_port=$(cat ${RSYSLOG_DYNNAME}.otel_port.file)
if [ -z "$otel_port" ]; then
	echo "ERROR: OTEL Collector port is empty"
	error_exit 1
fi
echo "Using OTEL Collector port: $otel_port"

# Start proxy server (no authentication)
proxy_server_py="$srcdir/omotlp_proxy_server.py"
if [ ! -f "$proxy_server_py" ]; then
	echo "ERROR: Proxy server script not found: $proxy_server_py"
	error_exit 1
fi

proxy_port_file="${RSYSLOG_DYNNAME}.proxy_port.file"
proxy_data_file="${RSYSLOG_DYNNAME}.proxy_data.json"

# Get a free port for the proxy server
PROXY_PORT=$(get_free_port)
export PROXY_PORT

echo "Starting proxy server on port: $PROXY_PORT..."
python3 "$proxy_server_py" \
	--port "$PROXY_PORT" \
	--port-file "$proxy_port_file" \
	--target-host 127.0.0.1 \
	--target-port "$otel_port" \
	--data-file "$proxy_data_file" \
	> "${RSYSLOG_DYNNAME}.proxy.log" 2>&1 &
proxy_pid=$!

# Wait for proxy to start and verify port file
wait_file_exists "$proxy_port_file" 10
if [ ! -f "$proxy_port_file" ]; then
	echo "ERROR: Proxy server did not create port file"
	kill $proxy_pid 2>/dev/null
	error_exit 1
fi

proxy_port=$(cat "$proxy_port_file")
if [ -z "$proxy_port" ]; then
	echo "ERROR: Proxy port is empty"
	kill $proxy_pid 2>/dev/null
	error_exit 1
fi

# Verify the port matches what we requested
if [ "$proxy_port" != "$PROXY_PORT" ]; then
	echo "WARNING: Proxy port mismatch (requested: $PROXY_PORT, got: $proxy_port)"
fi

echo "Proxy server started on port: $proxy_port (pid: $proxy_pid)"

# Function to stop proxy server
stop_proxy() {
	if [ -n "$proxy_pid" ] && kill -0 "$proxy_pid" 2>/dev/null; then
		echo "Stopping proxy server (pid: $proxy_pid)"
		kill "$proxy_pid" 2>/dev/null
		wait "$proxy_pid" 2>/dev/null || true
	fi
}

# Ensure proxy is stopped on exit
trap 'stop_proxy' EXIT

# Test 1: Proxy without authentication
echo "=== Test 1: Proxy without authentication ==="
generate_conf
add_conf '
template(name="otlpBody" type="string" string="msgnum:%msg:F,58:2%")

module(load="../plugins/omotlp/.libs/omotlp")

if $msg contains "msgnum:" then
	action(
		name="omotlp-proxy"
		type="omotlp"
		template="otlpBody"
		endpoint="http://127.0.0.1:'$otel_port'"
		path="/v1/logs"
		proxy="http://127.0.0.1:'$proxy_port'"
		batch.max_items="50"
		batch.timeout.ms="1000"
	)
'
startup
injectmsg
shutdown_when_empty
wait_shutdown

# Stop OTEL Collector to flush data
stop_otel_collector

# Give time for data to flush
if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
	$TESTTOOL_DIR/msleep 1000
else
	sleep 1
fi

# Verify proxy was used
if [ -f "$proxy_data_file" ]; then
	proxy_requests=$(python3 -c "import json; f=open('$proxy_data_file'); data=json.load(f); print(len(data)); f.close()" 2>/dev/null || echo "0")
	if [ "$proxy_requests" -eq "0" ]; then
		echo "ERROR: Proxy server did not receive any requests"
		cat "${RSYSLOG_DYNNAME}.proxy.log"
		error_exit 1
	fi
	echo "Proxy server received $proxy_requests requests"
else
	echo "WARNING: Proxy data file not found, but continuing..."
fi

# Extract and verify OTEL Collector output
otel_collector_get_data

# Use the output file path set by prepare_otel_collector
if [ -z "$OTEL_OUTPUT_FILE" ]; then
	# Fallback: construct path like other tests
	test_dir="${srcdir:-$(pwd)}"
	if [[ "$test_dir" != /* ]]; then
		test_dir="$(cd "$test_dir" && pwd)"
	fi
	otel_output_file="$test_dir/${RSYSLOG_DYNNAME}.otel-output.json"
else
	otel_output_file="$OTEL_OUTPUT_FILE"
fi

python3 - "$otel_output_file" <<'PY'
import json
import sys

path = sys.argv[1]
try:
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
    sys.stderr.write(f"omotlp-proxy: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not records:
    sys.stderr.write("omotlp-proxy: OTLP output did not contain any logRecords\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-proxy: successfully received {len(records)} log records via proxy\n")
PY

seq_check

# Test 2: Proxy with authentication
echo "=== Test 2: Proxy with authentication ==="

# Stop current proxy and start new one with auth
stop_proxy
sleep 1

proxy_port_file2="${RSYSLOG_DYNNAME}.proxy2_port.file"
proxy_data_file2="${RSYSLOG_DYNNAME}.proxy2_data.json"

# Get a free port for the authenticated proxy server
PROXY_PORT2=$(get_free_port)
export PROXY_PORT2

echo "Starting authenticated proxy server on port: $PROXY_PORT2..."
python3 "$proxy_server_py" \
	--port "$PROXY_PORT2" \
	--port-file "$proxy_port_file2" \
	--target-host 127.0.0.1 \
	--target-port "$otel_port" \
	--require-auth \
	--user "testuser" \
	--password "testpass" \
	--data-file "$proxy_data_file2" \
	> "${RSYSLOG_DYNNAME}.proxy2.log" 2>&1 &
proxy_pid2=$!

wait_file_exists "$proxy_port_file2" 10
proxy_port2=$(cat "$proxy_port_file2")

# Verify the port matches what we requested
if [ "$proxy_port2" != "$PROXY_PORT2" ]; then
	echo "WARNING: Authenticated proxy port mismatch (requested: $PROXY_PORT2, got: $proxy_port2)"
fi

echo "Authenticated proxy server started on port: $proxy_port2 (pid: $proxy_pid2)"

# Update trap to stop both proxies
stop_proxy2() {
	stop_proxy
	if [ -n "$proxy_pid2" ] && kill -0 "$proxy_pid2" 2>/dev/null; then
		echo "Stopping authenticated proxy server (pid: $proxy_pid2)"
		kill "$proxy_pid2" 2>/dev/null
		wait "$proxy_pid2" 2>/dev/null || true
	fi
}
trap 'stop_proxy2' EXIT

# Restart OTEL Collector for second test
prepare_otel_collector
start_otel_collector
otel_port2=$(cat ${RSYSLOG_DYNNAME}.otel_port.file)

generate_conf
add_conf '
template(name="otlpBody" type="string" string="msgnum:%msg:F,58:2%")

module(load="../plugins/omotlp/.libs/omotlp")

if $msg contains "msgnum:" then
	action(
		name="omotlp-proxy-auth"
		type="omotlp"
		template="otlpBody"
		endpoint="http://127.0.0.1:'$otel_port2'"
		path="/v1/logs"
		proxy="http://127.0.0.1:'$proxy_port2'"
		proxy.user="testuser"
		proxy.password="testpass"
		batch.max_items="50"
		batch.timeout.ms="1000"
	)
'
startup
injectmsg
shutdown_when_empty
wait_shutdown

stop_otel_collector
stop_proxy2

# Verify authenticated proxy was used
if [ -f "$proxy_data_file2" ]; then
	proxy_requests2=$(python3 -c "import json; f=open('$proxy_data_file2'); data=json.load(f); print(len(data)); f.close()" 2>/dev/null || echo "0")
	if [ "$proxy_requests2" -eq "0" ]; then
		echo "ERROR: Authenticated proxy server did not receive any requests"
		cat "${RSYSLOG_DYNNAME}.proxy2.log"
		error_exit 1
	fi
	echo "Authenticated proxy server received $proxy_requests2 requests"
fi

exit_test

