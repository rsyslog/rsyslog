#!/bin/bash
# Validate mmutf8fix replacementSequence for invalid UTF-8 in message, tag,
# and structured data fields. The oracle is the configured omfile destination
# after synchronized shutdown, proving the rewritten fields reached the normal
# output path. Invalid bytes are replaced once per byte with the configured
# UTF-8 replacement sequence.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmutf8fix/.libs/mmutf8fix")
module(load="../plugins/imtcp/.libs/imtcp")

parser(name="custom.rfc3164" type="pmrfc3164" force.tagEndingByColon="on")
template(name="sdoutfmt" type="string" string="%structured-data%|%msg%\n")
template(name="tagoutfmt" type="string" string="%syslogtag%|%msg%\n")

input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_sd_port"
      ruleset="sdtest")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_tag_port"
      ruleset="tagtest")

ruleset(name="sdtest") {
	action(type="mmutf8fix" replacementSequence="\357\277\275")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="sdoutfmt")
}

ruleset(name="tagtest" parser="custom.rfc3164") {
	action(type="mmutf8fix" replacementSequence="\357\277\275")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="tagoutfmt")
}'

startup
wait_file_exists "$RSYSLOG_DYNNAME.tcpflood_sd_port"
wait_file_exists "$RSYSLOG_DYNNAME.tcpflood_tag_port"
printf '<134>1 2026-06-01T00:00:00Z host app proc msgid [test@32473 dirty="Brain\xa0Twist"] bad\xc0\x80 utf8\n' \
  > "$RSYSLOG_DYNNAME.input"
tcpflood -p"$(cat "$RSYSLOG_DYNNAME.tcpflood_sd_port")" -m1 -I "$RSYSLOG_DYNNAME.input"
printf '<129>Mar 10 01:00:00 host bad\xc0tag: tag payload\n' > "$RSYSLOG_DYNNAME.input"
tcpflood -p"$(cat "$RSYSLOG_DYNNAME.tcpflood_tag_port")" -m1 -I "$RSYSLOG_DYNNAME.input"
rm -f "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown

{
  printf '[test@32473 dirty="Brain\357\277\275Twist"]|bad\357\277\275\357\277\275 utf8\n'
  printf 'bad\357\277\275tag:| tag payload\n'
} > "$RSYSLOG_OUT_LOG.expect"

cmp_exact_file "$RSYSLOG_OUT_LOG.expect" "$RSYSLOG_OUT_LOG"
exit_test
