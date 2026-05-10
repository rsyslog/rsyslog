#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imkubernetes ../contrib/imkubernetes

pwd=$( pwd )
logdir="${RSYSLOG_DYNNAME}.spool/containers"
logfile="${logdir}/pod-name2_namespace-name2_container-b-deadbeef.log"

generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../contrib/imkubernetes/.libs/imkubernetes"
       LogFileGlob="'$pwd/$logdir'/*.log"
       PollingInterval="1"
       EnrichKubernetes="off")

template(name="outfmt" type="list" option.jsonf="on") {
    property(outname="msg" name="msg" format="jsonf")
    property(outname="namespace" name="$!kubernetes!namespace_name" format="jsonf")
    property(outname="pod" name="$!kubernetes!pod_name" format="jsonf")
    property(outname="container" name="$!kubernetes!container_name" format="jsonf")
    property(outname="stream" name="$!kubernetes!stream" format="jsonf")
    property(outname="format" name="$!kubernetes!log_format" format="jsonf")
    property(outname="container_id" name="$!docker!container_id" format="jsonf")
}

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'

mkdir -p "$logdir"
startup

cat > "$logfile" <<'EOF'
{"log":"docker stdout line\n","stream":"stdout","time":"2026-04-20T10:01:00.123456789Z"}
{"log":"docker stderr line\n","stream":"stderr","time":"2026-04-20T10:01:01.123456789Z"}
EOF

wait_file_lines "$RSYSLOG_OUT_LOG" 3
wait_queueempty
shutdown_when_empty
wait_shutdown

$PYTHON - "$RSYSLOG_OUT_LOG" <<'PY'
import json
import sys

path = sys.argv[1]
records = [json.loads(line) for line in open(path)]
records = [item for item in records if item.get("format") == "docker_json"]
assert len(records) == 2, records
by_msg = {item["msg"]: item for item in records}

stdout = by_msg["docker stdout line"]
assert stdout["namespace"] == "namespace-name2"
assert stdout["pod"] == "pod-name2"
assert stdout["container"] == "container-b"
assert stdout["stream"] == "stdout"
assert stdout["format"] == "docker_json"
assert stdout["container_id"] == "deadbeef"

stderr = by_msg["docker stderr line"]
assert stderr["stream"] == "stderr"
assert stderr["format"] == "docker_json"
assert stderr["container_id"] == "deadbeef"
PY

exit_test
