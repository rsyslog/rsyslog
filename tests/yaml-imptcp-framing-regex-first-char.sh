#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# YAML companion for imptcp_framing_regex-first-char.sh. The imptcp input with
# framing.delimiter.regex is loaded from a YAML include, and the omfile output
# after synchronized shutdown must keep the first character of each frame.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
global:
  parser.escapeControlCharactersOnReceive: "off"

modules:
  - load: "../plugins/imptcp/.libs/imptcp"

inputs:
  - type: imptcp
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.tcpflood_port"
    ruleset: remote
    framing.delimiter.regex: "^[0-9]"

templates:
  - name: outfmt
    type: string
    string: "NEWMSG: %rawmsg%\n"

rulesets:
  - name: remote
    script: |
      action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF

startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
tcpflood -B -I ${srcdir}/testsuites/imptcp_framing_regex-first-char.testdata
shutdown_when_empty
wait_shutdown
export EXPECTED='NEWMSG: 2026-10-23 10:00:01 host app: java.lang.IllegalStateException: boom
	at com.example.Foo.bar(Foo.java:10)
	at com.example.Foo.main(Foo.java:5)
NEWMSG: 2026-10-23 10:00:02 host app: second record
NEWMSG: 2026-10-23 10:00:03 host app: third record'
cmp_exact
exit_test
