#!/bin/bash
# Verify imkubernetes can be configured through the YAML frontend. The test
# tails a Kubernetes /var/log/containers-style Docker json-file record with API
# enrichment disabled; output metadata proves the YAML module parameters reached
# imkubernetes and the parsed record flowed through the regular queue.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imkubernetes ../contrib/imkubernetes

pwd=$( pwd )
logdir="${RSYSLOG_DYNNAME}.spool/containers"
logfile="${logdir}/pod-name3_namespace-name3_container-c-feedface.log"

generate_conf --yaml-only
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../contrib/imkubernetes/.libs/imkubernetes"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imkubernetes'
add_yaml_conf '    logfileglob: "'$pwd/$logdir'/*.log"'
add_yaml_conf '    pollinginterval: 1'
add_yaml_conf '    enrichkubernetes: off'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'templates:'
add_yaml_conf '  - name: outfmt'
add_yaml_conf '    type: string'
add_yaml_conf '    string: "%msg%|%$!kubernetes!namespace_name%|%$!kubernetes!pod_name%|%$!kubernetes!container_name%|%$!kubernetes!stream%|%$!kubernetes!log_format%|%$!docker!container_id%\n"'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    script: |'
add_yaml_conf '      action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")'

mkdir -p "$logdir"
startup

cat > "$logfile" <<'EOF'
{"log":"yaml docker line\n","stream":"stdout","time":"2026-04-20T10:02:00.123456789Z"}
EOF

wait_file_lines "$RSYSLOG_OUT_LOG" 1
wait_queueempty
shutdown_when_empty
wait_shutdown

content_check 'yaml docker line|namespace-name3|pod-name3|container-c|stdout|docker_json|feedface'
exit_test
