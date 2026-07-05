#!/bin/bash
# YAML companion for imtcp-spacelf-escape.sh. The parser options are loaded
# from a YAML include, while success is proven by the configured omfile output
# after synchronized shutdown: LF is rewritten to a space, and the earlier tab,
# CR, BEL, BS, and 8-bit bytes still escape correctly.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
global:
  parser.spaceLFOnReceive: "on"
  parser.escapeControlCharactersOnReceive: "on"
  parser.escapeControlCharacterTab: "on"
  parser.escape8BitCharactersOnReceive: "on"
  parser.escapeControlCharactersCStyle: "off"

modules:
  - load: "../plugins/imtcp/.libs/imtcp"

inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "${RSYSLOG_DYNNAME}.tcpflood_port"
    supportOctetCountedFraming: "on"
    ruleset: remote

templates:
  - name: outfmt
    type: string
    string: "%msg%\n"

rulesets:
  - name: remote
    script: |
      action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF

payload=$'<13>Oct 11 22:14:15 host tag: tab\tline carriage\rtest beep\abackspace\btest high \302\251 later\nlf'
tcpflood_bin="${TCPFLOOD_BIN:-./tcpflood}"

startup
assign_tcpflood_port "$RSYSLOG_DYNNAME.tcpflood_port"
"$tcpflood_bin" -p"$TCPFLOOD_PORT" -m1 -O -M "$payload"
shutdown_when_empty
wait_shutdown

export EXPECTED=' tab#011line carriage#015test beep#007backspace#010test high #302#251 later lf'
cmp_exact
exit_test
