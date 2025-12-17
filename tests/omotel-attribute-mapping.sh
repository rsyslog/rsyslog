#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
## omotel-attribute-mapping.sh -- test custom attribute mapping for omotel module
##
## Tests that custom attribute mappings (attributeMap) correctly remap
## rsyslog properties to custom OTLP attribute names.

. ${srcdir:=.}/diag.sh init

# Check if omotel module is available
require_plugin omotel

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

# Create input file with syslog messages containing various syslog fields
cat > ${RSYSLOG_DYNNAME}.input << 'INPUTFILE'
<14>1 2024-01-01T12:00:00.000Z testhost1 testapp1 12345 msgid001 - test message 00000001
<14>1 2024-01-01T12:00:00.000Z testhost2 testapp2 67890 msgid002 - test message 00000002
<14>1 2024-01-01T12:00:00.000Z testhost3 testapp3 11111 msgid003 - test message 00000003
<14>1 2024-01-01T12:00:00.000Z testhost4 testapp4 22222 msgid004 - test message 00000004
<14>1 2024-01-01T12:00:00.000Z testhost5 testapp5 33333 msgid005 - test message 00000005
<14>1 2024-01-01T12:00:00.000Z testhost6 testapp6 44444 msgid006 - test message 00000006
<14>1 2024-01-01T12:00:00.000Z testhost7 testapp7 55555 msgid007 - test message 00000007
<14>1 2024-01-01T12:00:00.000Z testhost8 testapp8 66666 msgid008 - test message 00000008
<14>1 2024-01-01T12:00:00.000Z testhost9 testapp9 77777 msgid009 - test message 00000009
<14>1 2024-01-01T12:00:00.000Z testhost10 testapp10 88888 msgid010 - test message 00000010
INPUTFILE

generate_conf
add_conf '
template(name="otlpBody" type="string" string="%msg%")

module(load="../plugins/omotel/.libs/omotel")

# Export with custom attribute mapping
action(
	name="omotel-http"
	type="omotel"
	template="otlpBody"
	endpoint="http://127.0.0.1:'$otel_port'"
	path="/v1/logs"
	attributeMap="{ \"hostname\": \"host.name\", \"appname\": \"service.name\", \"procid\": \"process.pid\", \"msgid\": \"message.id\", \"facility\": \"log.syslog.facility.code\" }"
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
    sys.stderr.write(f"omotel-attribute-mapping: failed to parse OTLP output: {exc}\n")
    sys.exit(1)

if not records:
    sys.stderr.write("omotel-attribute-mapping: OTLP output did not contain any logRecords\n")
    sys.exit(1)

sys.stdout.write(f"omotel-attribute-mapping: found {len(records)} log records in OTLP output\n")

# Filter records to only test messages (contain "test message" in body)
test_records = []
for record in records:
    body = record.get("body", {})
    body_str = body.get("stringValue", "") if isinstance(body, dict) else str(body)
    if "test message" in body_str:
        test_records.append(record)

if len(test_records) < 10:
    sys.stderr.write(f"omotel-attribute-mapping: expected at least 10 test records, found {len(test_records)}\n")
    sys.exit(1)

sys.stdout.write(f"omotel-attribute-mapping: filtered to {len(test_records)} test records\n")

# Expected test data (from input file)
expected_test_data = [
    {"hostname": "testhost1", "appname": "testapp1", "procid": "12345", "msgid": "msgid001", "facility": 1},
    {"hostname": "testhost2", "appname": "testapp2", "procid": "67890", "msgid": "msgid002", "facility": 1},
    {"hostname": "testhost3", "appname": "testapp3", "procid": "11111", "msgid": "msgid003", "facility": 1},
    {"hostname": "testhost4", "appname": "testapp4", "procid": "22222", "msgid": "msgid004", "facility": 1},
    {"hostname": "testhost5", "appname": "testapp5", "procid": "33333", "msgid": "msgid005", "facility": 1},
    {"hostname": "testhost6", "appname": "testapp6", "procid": "44444", "msgid": "msgid006", "facility": 1},
    {"hostname": "testhost7", "appname": "testapp7", "procid": "55555", "msgid": "msgid007", "facility": 1},
    {"hostname": "testhost8", "appname": "testapp8", "procid": "66666", "msgid": "msgid008", "facility": 1},
    {"hostname": "testhost9", "appname": "testapp9", "procid": "77777", "msgid": "msgid009", "facility": 1},
    {"hostname": "testhost10", "appname": "testapp10", "procid": "88888", "msgid": "msgid010", "facility": 1},
]

# Expected custom attribute names (from attributeMap configuration)
expected_attr_names = {
    "host.name",      # mapped from "hostname"
    "service.name",   # mapped from "appname"
    "process.pid",    # mapped from "procid"
    "message.id",     # mapped from "msgid"
    "log.syslog.facility.code"  # mapped from "facility"
}

# Verify attribute mappings for each test record
errors = []
for idx, record in enumerate(test_records[:10]):  # Only check first 10 test records
    if "attributes" not in record:
        errors.append(f"record {idx}: missing attributes array")
        continue
    
    attrs = record["attributes"]
    attr_dict = {}
    
    # Parse attributes into a dictionary
    for attr_entry in attrs:
        key = attr_entry.get("key")
        value_obj = attr_entry.get("value", {})
        
        # Extract value based on type
        if "stringValue" in value_obj:
            attr_dict[key] = ("string", value_obj["stringValue"])
        elif "intValue" in value_obj:
            attr_dict[key] = ("int", value_obj["intValue"])
        else:
            attr_dict[key] = ("unknown", value_obj)
    
    # Get expected values for this record
    if idx >= len(expected_test_data):
        continue
    expected = expected_test_data[idx]
    
    # Check that custom attribute names are present (when values exist)
    for expected_attr in expected_attr_names:
        # Only require attribute if the source field has a value
        source_field = None
        if expected_attr == "host.name" and expected["hostname"]:
            source_field = "hostname"
        elif expected_attr == "service.name" and expected["appname"]:
            source_field = "appname"
        elif expected_attr == "process.pid" and expected["procid"]:
            source_field = "procid"
        elif expected_attr == "message.id" and expected["msgid"]:
            source_field = "msgid"
        elif expected_attr == "log.syslog.facility.code":
            source_field = "facility"
        
        if source_field and expected_attr not in attr_dict:
            errors.append(f"record {idx}: missing custom attribute '{expected_attr}' (source: {source_field})")
    
    # Check that default attribute names are NOT present for mapped properties
    # These should have been replaced by custom mappings
    mapped_defaults = {
        "log.syslog.hostname": "host.name",
        "log.syslog.appname": "service.name",
        "log.syslog.procid": "process.pid",
        "log.syslog.msgid": "message.id",
        "log.syslog.facility": "log.syslog.facility.code"
    }
    for default_attr, custom_attr in mapped_defaults.items():
        if default_attr in attr_dict:
            errors.append(f"record {idx}: found default attribute '{default_attr}' but should use custom mapping '{custom_attr}'")
    
    # Verify specific mappings with expected values
    if "host.name" in attr_dict:
        _, hostname = attr_dict["host.name"]
        if hostname != expected["hostname"]:
            errors.append(f"record {idx}: host.name mismatch: expected '{expected['hostname']}', got '{hostname}'")
    
    if "service.name" in attr_dict:
        _, appname = attr_dict["service.name"]
        if appname != expected["appname"]:
            errors.append(f"record {idx}: service.name mismatch: expected '{expected['appname']}', got '{appname}'")
    
    if "process.pid" in attr_dict:
        _, procid = attr_dict["process.pid"]
        if procid != expected["procid"]:
            errors.append(f"record {idx}: process.pid mismatch: expected '{expected['procid']}', got '{procid}'")
    
    if "message.id" in attr_dict:
        _, msgid = attr_dict["message.id"]
        if msgid != expected["msgid"]:
            errors.append(f"record {idx}: message.id mismatch: expected '{expected['msgid']}', got '{msgid}'")
    
    if "log.syslog.facility.code" in attr_dict:
        _, facility = attr_dict["log.syslog.facility.code"]
        # Convert to int for comparison (JSON may return different numeric types)
        facility_int = int(facility) if facility is not None else None
        expected_facility = int(expected["facility"])
        if facility_int != expected_facility:
            errors.append(f"record {idx}: log.syslog.facility.code mismatch: expected {expected_facility}, got {facility_int}")
    
    sys.stdout.write(f"omotel-attribute-mapping: record {idx}: verified {len(attr_dict)} attributes\n")

if errors:
    sys.stderr.write("omotel-attribute-mapping: attribute mapping verification errors:\n")
    for error in errors:
        sys.stderr.write(f"  - {error}\n")
    sys.exit(1)

sys.stdout.write(f"omotel-attribute-mapping: successfully verified custom attribute mappings for {len(test_records[:10])} test records\n")
PY

exit_test

