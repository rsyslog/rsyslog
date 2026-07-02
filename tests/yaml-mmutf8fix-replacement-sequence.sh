#!/bin/bash
# Verify mmutf8fix replacementSequence in a YAML-loaded configuration. The
# ruleset body is supplied via YAML script so message-modification action
# ordering matches the RainerScript action path, and success is proven by the
# configured omfile output after synchronized shutdown.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp

export NUMMESSAGES=1
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
include(file="'${RSYSLOG_DYNNAME}'.yaml")
input(type="imtcp" port="0" listenPortFileName="'${RSYSLOG_DYNNAME}'.tcpflood_port"
      ruleset="main")
'

cat > "${RSYSLOG_DYNNAME}.yaml" << YAMLEOF
modules:
  - load: "../plugins/mmutf8fix/.libs/mmutf8fix"
  - load: "../plugins/imtcp/.libs/imtcp"

templates:
  - name: outfmt
    type: string
    string: "%msg%\\n"

rulesets:
  - name: main
    script: |
      action(type="mmutf8fix" replacementSequence="\xef\xbf\xbd")
      action(type="omfile" file="${RSYSLOG_OUT_LOG}" template="outfmt")
YAMLEOF

startup
printf '<129>Mar 10 01:00:00 host tag: has\xc0invalid\n' > "$RSYSLOG_DYNNAME.input"
tcpflood -m1 -I "$RSYSLOG_DYNNAME.input"
rm -f "$RSYSLOG_DYNNAME.input"
wait_file_lines "$RSYSLOG_OUT_LOG" 1 60
shutdown_when_empty
wait_shutdown

printf ' has\357\277\275invalid\n' > "$RSYSLOG_OUT_LOG.expect"
cmp_exact_file "$RSYSLOG_OUT_LOG.expect" "$RSYSLOG_OUT_LOG"
exit_test
