#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Adiscon GmbH.
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify one PutRecord request, SigV4 headers, base64 data and partition key.
# The server publishes its bound port before rsyslog starts; the captured
# request is the success oracle after synchronized shutdown. Readiness allows
# 20 seconds (200 100-ms polls) because broad CI can heavily load the runner.
. ${srcdir:=.}/diag.sh init
require_plugin omkinesis
command -v python3 >/dev/null 2>&1 || exit 77

port_file="${RSYSLOG_DYNNAME}.kinesis.port"
capture_file="${RSYSLOG_DYNNAME}.kinesis.capture"
python3 "${srcdir}/omkinesis-mock-server.py" "$port_file" "$capture_file" &
mock_pid=$!
trap 'kill "$mock_pid" 2>/dev/null || true' EXIT
wait_file_exists_for_process "$port_file" "$mock_pid" 200 "Kinesis mock" "${RSYSLOG_DYNNAME}.mock.stderr"
port=$(cat "$port_file")

export AWS_ACCESS_KEY_ID=mock-access
export AWS_SECRET_ACCESS_KEY=mock-secret
export AWS_SESSION_TOKEN=mock-session

generate_conf
if [[ "$OMKINESIS_FORMAT" == yaml ]]; then
    cat >"${RSYSLOG_DYNNAME}.kinesis.yaml" <<YAML_EOF
version: 2
templates:
  - name: kinesisFmt
    type: string
    string: "kinesis-message"
  - name: kinesisKey
    type: string
    string: "test-partition"
rulesets:
  - name: main
    statements:
      - type: omkinesis
        stream: test-stream
        region: us-east-1
        partition_key_template: kinesisKey
        template: kinesisFmt
        timeout: 2
        endpoint: "http://127.0.0.1:${port}/"
YAML_EOF
    add_conf '
module(load="../plugins/omkinesis/.libs/omkinesis")
include(file="'${RSYSLOG_DYNNAME}'.kinesis.yaml")
if $msg contains "msgnum:" then call main
'
else
    add_conf '
module(load="../plugins/omkinesis/.libs/omkinesis")
template(name="kinesisFmt" type="string" string="kinesis-message")
if $msg contains "msgnum:" then action(type="omkinesis" stream="test-stream"
  region="us-east-1" partition_key="test-partition" template="kinesisFmt"
  timeout="2" endpoint="http://127.0.0.1:'$port'/")
'
fi
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
wait "$mock_pid" || error_exit 1
trap - EXIT
python3 - "$capture_file" <<'PY' || error_exit 1
import base64
import json
import sys

with open(sys.argv[1], encoding="utf-8") as capture:
    request = json.load(capture)
headers = {key.lower(): value for key, value in request["headers"].items()}
assert request["path"] == "/", request
assert request["body"]["StreamName"] == "test-stream", request
assert request["body"]["PartitionKey"] == "test-partition", request
assert base64.b64decode(request["body"]["Data"]) == b"kinesis-message", request
assert headers["x-amz-target"] == "Kinesis_20131202.PutRecord", headers
assert "Credential=mock-access/" in headers["authorization"], headers
assert headers["x-amz-security-token"] == "mock-session", headers
PY
exit_test
