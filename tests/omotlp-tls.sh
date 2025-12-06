#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-tls.sh -- TLS/mTLS support test for omotlp module
##
## Tests that omotlp correctly establishes HTTPS connections with TLS
## certificate verification, and optionally with mutual TLS (mTLS) using
## client certificates.

. ${srcdir:=.}/diag.sh init

# Check if omotlp module is available
require_plugin omotlp

export NUMMESSAGES=500
export EXTRA_EXIT=otel
export SEQ_CHECK_OPTIONS=-d

# Check for required certificates
if [ ! -f "$srcdir/testsuites/x.509/ca.pem" ]; then
	echo "ERROR: CA certificate not found: $srcdir/testsuites/x.509/ca.pem"
	echo "Skipping TLS test - certificates not available"
	error_exit 77
fi

if [ ! -f "$srcdir/testsuites/x.509/client-cert.pem" ]; then
	echo "ERROR: Client certificate not found: $srcdir/testsuites/x.509/client-cert.pem"
	echo "Skipping TLS test - certificates not available"
	error_exit 77
fi

if [ ! -f "$srcdir/testsuites/x.509/client-key.pem" ]; then
	echo "ERROR: Client key not found: $srcdir/testsuites/x.509/client-key.pem"
	echo "Skipping TLS test - certificates not available"
	error_exit 77
fi

# We need a server certificate for OTEL Collector
# For this test, we'll use the client cert as server cert (self-signed test scenario)
# In a real deployment, you'd have separate server certificates
server_cert="$srcdir/testsuites/x.509/client-cert.pem"
server_key="$srcdir/testsuites/x.509/client-key.pem"
ca_cert="$srcdir/testsuites/x.509/ca.pem"
client_cert="$srcdir/testsuites/x.509/client-cert.pem"
client_key="$srcdir/testsuites/x.509/client-key.pem"

# Download and prepare OTEL Collector (extracts binary and sets up work directory)
download_otel_collector
prepare_otel_collector

# Override config with TLS-enabled version
dep_work_dir=$(readlink -f .dep_wrk 2>/dev/null || echo "$(pwd)/.dep_wrk")
otelcol_work_dir="$dep_work_dir/otelcol-${RSYSLOG_DYNNAME}"

# Copy certificates to OTEL Collector work directory
cp "$server_cert" "$otelcol_work_dir/server-cert.pem"
cp "$server_key" "$otelcol_work_dir/server-key.pem"
cp "$ca_cert" "$otelcol_work_dir/ca.pem"

# Get absolute path for output file
test_dir="${srcdir:-$(pwd)}"
if [[ "$test_dir" != /* ]]; then
	test_dir="$(cd "$test_dir" && pwd)"
fi
otel_output_file="$test_dir/${RSYSLOG_DYNNAME}.otel-output.json"
otel_output_file_abs=$(cd "$(dirname "$otel_output_file")" && pwd)/$(basename "$otel_output_file")

# Override config with TLS-enabled version
cat > "$otelcol_work_dir/config.yaml" <<EOF
receivers:
  otlp:
    protocols:
      http:
        endpoint: 0.0.0.0:${OTEL_COLLECTOR_PORT}
        tls:
          cert_file: server-cert.pem
          key_file: server-key.pem
          # For mTLS, require client certificates
          client_ca_file: ca.pem

exporters:
  file:
    path: ${otel_output_file_abs}
    format: json
    flush_interval: 1s

service:
  telemetry:
    metrics:
      address: 0.0.0.0:${OTEL_METRICS_PORT}
  pipelines:
    logs:
      receivers: [otlp]
      exporters: [file]
EOF

echo "OTEL Collector TLS config created:"
cat "$otelcol_work_dir/config.yaml"

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
echo "Using OTEL Collector port: $otel_port (HTTPS)"

generate_conf
add_conf '
template(name="otlpBody" type="string" string="msgnum:%msg:F,58:2%")

module(load="../plugins/omotlp/.libs/omotlp")

if $msg contains "msgnum:" then
	action(
		name="omotlp-https"
		type="omotlp"
		template="otlpBody"
		endpoint="https://127.0.0.1:'$otel_port'"
		path="/v1/logs"
		batch.max_items="50"
		batch.timeout.ms="1000"
		tls.cacert="'$ca_cert'"
		tls.cert="'$client_cert'"
		tls.key="'$client_key'"
		tls.verify_hostname="off"
		tls.verify_peer="on"
	)
'

startup
injectmsg
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

# Verify messages were received
python3 - "$RSYSLOG_DYNNAME.otel-output.json" <<'PY'
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
    sys.stderr.write(f"omotlp-tls: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not records:
    sys.stderr.write("omotlp-tls: OTLP output did not contain any logRecords\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-tls: successfully received {len(records)} log records via HTTPS/TLS\n")
PY

seq_check
exit_test

