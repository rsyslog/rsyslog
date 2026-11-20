#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify an invalid imkubernetes defaultfacility (nosuchfacility) keeps the default
# facility ordinal 1 for emitted Docker records.
. ${srcdir:=.}/diag.sh init
require_plugin imkubernetes ../contrib/imkubernetes

pwd=$( pwd )
logdir="${RSYSLOG_DYNNAME}.spool/containers"
logfile="${logdir}/pod_namespace_container-feedface.log"
generate_conf
add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../contrib/imkubernetes/.libs/imkubernetes"
       LogFileGlob="'$pwd/$logdir'/*.log"
       PollingInterval="1"
       EnrichKubernetes="off" DefaultFacility="nosuchfacility")
template(name="outfmt" type="string" string="%msg%|%syslogfacility%\n")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
mkdir -p "$logdir"
startup
cat > "$logfile" <<'EOF'
{"log":"invalid facility fallback\n","stream":"stdout","time":"2026-04-20T10:01:00.123456789Z"}
EOF
wait_file_lines "$RSYSLOG_OUT_LOG" 1
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'invalid facility fallback|1'
exit_test
