#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test: rawmsg-after-pri on raw messages shorter than a full PRI.
# getRawMsgAfterPRI() used to probe rawmsg[2..4] without checking the raw
# length. Short messages live in the non-zeroed inline szRawMsg buffer, so
# for "<1" the probe read stale bytes; a stale '>' could yield a negative
# property length.
# Oracle: every short frame comes back unchanged through both the template
# and the RainerScript variable path, and rsyslogd shuts down cleanly. The
# stale-byte content depends on the heap, so a pre-fix build fails this test
# reliably only under valgrind or with ASAN_OPTIONS=malloc_fill_byte=62.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
	ruleset="rs")

template(name="outfmt" type="string" string="%rawmsg-after-pri%|%$!a%\n")
ruleset(name="rs") {
	set $!a = $rawmsg-after-pri;
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
printf '<13>full\n<\n<1\n<12\n<1>\n<123\n' > "$RSYSLOG_DYNNAME.input"
tcpflood -B -I "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown
export EXPECTED='full|full
<|<
<1|<1
<12|<12
|
<123|<123'
cmp_exact
exit_test
