#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
# Verify that YAML configures both maxMessageSize and imklog's LogPath.
# A 16 KiB record exceeds the default limit. With a 256 KiB limit it must
# arrive intact. Wait for output before shutdown, then compare the full record.
. "${srcdir:=.}/diag.sh" init
require_yaml_support
require_plugin imklog

printf -v EXPECTED 'imklog-yaml:%016384d' 0
export EXPECTED
printf '%s\n' "$EXPECTED" > "$RSYSLOG_DYNNAME.kmsg"

generate_conf
cat > "$RSYSLOG_DYNNAME.imklog.yaml" <<EOF
global:
  maxMessageSize: 262144
modules:
  - load: "../plugins/imklog/.libs/imklog"
    logPath: "$RSYSLOG_DYNNAME.kmsg"
EOF
add_conf 'include(file="'${RSYSLOG_DYNNAME}'.imklog.yaml")
template(name="imklogOutput" type="string" string="%msg%\n")
if $msg startswith "imklog-yaml:" then
	action(type="omfile" template="imklogOutput" file="'$RSYSLOG_OUT_LOG'")
'
startup
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown
cmp_exact
exit_test
