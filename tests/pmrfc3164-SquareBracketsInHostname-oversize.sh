#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
#
# Regression test: with permit.SquareBracketsInHostname="on" a bracketed
# hostname that exactly fills bufParseHOSTNAME made pmrfc3164 append the
# closing bracket at the last usable slot and then write the string
# terminator one byte past the end of the stack buffer. Such a hostname
# must not be accepted, and rsyslog must not corrupt its stack.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="customparser")
parser(name="custom.rfc3164" type="pmrfc3164" permit.SquareBracketsInHostname="on")
template(name="outfmt" type="string" string="-%hostname%-\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'
# CONF_HOSTNAME_MAXSIZE is 512, so "[" + 510 chars + "]" is exactly the
# buffer size and used to trigger the off-by-one write.
oversize_host="[$(printf 'A%.0s' $(seq 1 510))]"
short_host="[192.0.2.1]"

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 $short_host tag: msgnum:1\""
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 $oversize_host tag: msgnum:2\""
shutdown_when_empty
wait_shutdown

# The first message yields the bracketed hostname. The second one is not a
# representable hostname, so pmrfc3164 falls back to the receiver's own
# hostname - the important part is that rsyslog survives and does not emit
# the oversized token as HOSTNAME.
if ! grep -qF -- "-$short_host-" $RSYSLOG_OUT_LOG; then
	echo "FAIL: bracketed hostname not parsed, $RSYSLOG_OUT_LOG is:"
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi
if grep -qF -- "-[AAAA" $RSYSLOG_OUT_LOG; then
	echo "FAIL: oversized bracketed hostname was accepted, $RSYSLOG_OUT_LOG is:"
	cat $RSYSLOG_OUT_LOG
	error_exit 1
fi
exit_test
