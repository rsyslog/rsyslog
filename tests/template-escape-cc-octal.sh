#!/bin/bash
# added 2026-06-14 by Rainer Gerhards; released under ASL 2.0
# Regression test for template option escape-cc: it must use the same
# octal #ooo encoding as receive-side control-character escaping. The
# oracle is the final omfile output after synchronized shutdown, proving
# that the escaped message reached the configured destination.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
global(parser.escapeControlCharactersOnReceive="off")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="ruleset1")

template(name="outfmt" type="string" string="%msg:::escape-cc%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG`
	       template="outfmt")
}

'
startup
tcpflood -m1 -M $'"<167>Mar  6 16:57:54 172.20.245.8 test: before CR\rafter CR"'
shutdown_when_empty
wait_shutdown

# shellcheck disable=SC2034 # consumed by cmp_exact from diag.sh
EXPECTED=' before CR#015after CR'
cmp_exact

if grep -q '#013' "$RSYSLOG_OUT_LOG"; then
	printf 'template escape-cc used decimal CR escaping instead of octal:\n'
	cat -n "$RSYSLOG_OUT_LOG"
	error_exit 1
fi

exit_test
