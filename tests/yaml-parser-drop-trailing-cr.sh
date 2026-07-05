#!/bin/bash
# Verify YAML support for parser.dropTrailingCROnReception using the same
# receive-path oracle as the RainerScript test: final omfile output after
# synchronized shutdown must not contain the escaped trailing CR.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
require_yaml_support

generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
global:
  parser.dropTrailingCROnReception: "on"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.tcpflood_port"
    ruleset: rs
templates:
  - name: outfmt
    type: string
    string: "%msg%\\n"
rulesets:
  - name: rs
    script: |
      action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF

startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
cr_msg=$(printf '"<167>Mar  6 16:57:54 172.20.245.8 test: payload\r"')
tcpflood -m1 -M "$cr_msg"
shutdown_when_empty
wait_shutdown

export EXPECTED=' payload'
cmp_exact

exit_test
