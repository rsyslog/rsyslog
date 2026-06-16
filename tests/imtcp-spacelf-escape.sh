#!/bin/bash
# Verifies that parser.spaceLFOnReceive does not mask other sanitization.
# The octet-counted input contains an embedded LF, other control bytes, and an
# 8-bit UTF-8 byte sequence. Success is proven by the configured omfile output:
# LF is rewritten to a space, while tab, CR, BEL, BS, and 8-bit bytes are still
# escaped after synchronized shutdown.
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(
	parser.spaceLFOnReceive="on"
	parser.escapeControlCharactersOnReceive="on"
	parser.escapeControlCharacterTab="on"
	parser.escape8BitCharactersOnReceive="on"
	parser.escapeControlCharactersCStyle="off"
)

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	supportOctetCountedFraming="on" ruleset="remote")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="remote") {
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

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
