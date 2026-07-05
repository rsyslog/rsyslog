#!/bin/bash
# Tests YAML list-template control-character handling for the modern
# property() parameter surface. The oracle is exact omfile output: a newline
# and tab in a message variable must be emitted as octal #012 and #011 when
# controlCharacters is set to escape-octal from YAML.
#
# Added 2026, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << 'YAMLEOF'
modules:
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: list
    elements:
      - constant:
          value: "cc_escape_octal="
      - property:
          name: "$!control"
          controlCharacters: escape-octal
      - constant:
          value: "\n"

rulesets:
  - name: main
    script: |
      set $!control = "a\nb\tc";
      action(type="omfile" template="outfmt" file="${RSYSLOG_OUT_LOG}")
YAMLEOF
sed -i "s|\${RSYSLOG_OUT_LOG}|${RSYSLOG_OUT_LOG}|g" "${RSYSLOG_DYNNAME}.yaml"

add_conf '
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'
startup
tcpflood -m $NUMMESSAGES
shutdown_when_empty
wait_shutdown

export EXPECTED='cc_escape_octal=a#012b#011c'
cmp_exact
exit_test
