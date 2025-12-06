#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotlp-compression.sh -- compression and resource config test for omotlp module
##
## Tests that omotlp correctly sends gzip-compressed payloads
## and that OTEL Collector can decode them. Also tests resource parameter
## configuration with string, integer, double, and boolean attributes.

. ${srcdir:=.}/diag.sh init

export NUMMESSAGES=2000
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
		batch.max_items="10"
		batch.timeout.ms="1000"
		compression="gzip"
		resource="{ \"service.name\": \"test-service\", \"service.instance.id\": \"test-instance-123\", \"service.version\": \"1.2.3\", \"deployment.environment\": \"test\", \"custom.string.attr\": \"test-value\", \"custom.int.attr\": 42, \"custom.double.attr\": 3.14159, \"custom.bool.attr\": true }"
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

# Verify resource attributes
python3 - "$RSYSLOG_DYNNAME.otel-output.json" <<'PY'
import json
import sys

path = sys.argv[1]
try:
    # OTEL Collector file exporter writes newline-delimited JSON (one JSON object per line)
    payloads = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            payload = json.loads(line)
            if "resourceLogs" in payload:
                payloads.append(payload)
except Exception as exc:
    sys.stderr.write(f"omotlp-compression: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not payloads:
    sys.stderr.write("omotlp-compression: OTLP output did not contain any resourceLogs\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-compression: found {len(payloads)} payload(s)\n")

# Extract resource attributes from the first payload
resource_attrs = {}
found_resource = False

for payload in payloads:
    if "resourceLogs" in payload:
        for resource_log in payload["resourceLogs"]:
            if "resource" in resource_log and "attributes" in resource_log["resource"]:
                found_resource = True
                attrs = resource_log["resource"]["attributes"]
                sys.stdout.write(f"omotlp-compression: found {len(attrs)} resource attributes\n")
                
                # Parse attributes into a dictionary
                for attr_entry in attrs:
                    key = attr_entry.get("key")
                    value_obj = attr_entry.get("value", {})
                    
                    # Extract value based on type
                    if "stringValue" in value_obj:
                        resource_attrs[key] = ("string", value_obj["stringValue"])
                    elif "intValue" in value_obj:
                        resource_attrs[key] = ("int", value_obj["intValue"])
                    elif "doubleValue" in value_obj:
                        resource_attrs[key] = ("double", value_obj["doubleValue"])
                    elif "boolValue" in value_obj:
                        resource_attrs[key] = ("bool", value_obj["boolValue"])
                    else:
                        resource_attrs[key] = ("unknown", value_obj)
                
                # Only check the first resourceLogs entry
                break
        if found_resource:
            break

if not found_resource:
    sys.stderr.write("omotlp-compression: no resource attributes found in OTLP output\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-compression: extracted {len(resource_attrs)} resource attributes\n")

# Expected attributes (automatic + custom)
expected_attrs = {
    # Automatic attributes (always present)
    "service.name": ("string", "test-service"),  # Overridden by custom config
    "telemetry.sdk.name": ("string", "rsyslog-omotlp"),
    "telemetry.sdk.language": ("string", "C"),
    
    # Custom attributes from resource parameter
    "service.instance.id": ("string", "test-instance-123"),
    "service.version": ("string", "1.2.3"),
    "deployment.environment": ("string", "test"),
    "custom.string.attr": ("string", "test-value"),
    "custom.int.attr": ("int", 42),
    "custom.double.attr": ("double", 3.14159),
    "custom.bool.attr": ("bool", True),
}

# Verify expected attributes
errors = []
for key, (expected_type, expected_value) in expected_attrs.items():
    if key not in resource_attrs:
        errors.append(f"missing attribute: {key}")
        continue
    
    actual_type, actual_value = resource_attrs[key]
    
    if actual_type != expected_type:
        errors.append(f"attribute {key}: type mismatch (expected {expected_type}, got {actual_type})")
        continue
    
    if actual_value != expected_value:
        errors.append(f"attribute {key}: value mismatch (expected {expected_value}, got {actual_value})")
        continue
    
    sys.stdout.write(f"omotlp-compression: verified {key} = {actual_value} ({actual_type})\n")

# Check for telemetry.sdk.version (automatic, but version may vary)
if "telemetry.sdk.version" not in resource_attrs:
    errors.append("missing automatic attribute: telemetry.sdk.version")
else:
    sdk_type, sdk_version = resource_attrs["telemetry.sdk.version"]
    if sdk_type != "string" or not sdk_version:
        errors.append(f"telemetry.sdk.version has invalid value: {sdk_version}")
    else:
        sys.stdout.write(f"omotlp-compression: verified telemetry.sdk.version = {sdk_version}\n")

if errors:
    sys.stderr.write("omotlp-compression: resource attribute verification errors:\n")
    for error in errors:
        sys.stderr.write(f"  - {error}\n")
    sys.exit(1)

sys.stdout.write(f"omotlp-compression: successfully verified all resource attributes\n")
PY

seq_check
exit_test
