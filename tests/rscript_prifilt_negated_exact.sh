#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test for issue #1030. A standalone negated exact-priority PRI
# filter such as local4.!=debug must start from all priorities for the selected
# facility before clearing the excluded one, without resetting masks built by
# previous selectors in the same filter line. The oracle is omfile output after
# synchronized shutdown: local4.notice/info are written by the standalone
# selector, while a compound selector excludes both info and debug.
# A classic wildcard selector must apply the threshold to every facility
# before mail.none clears mail. A named facility list with exact priority
# must select only info from each named facility.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="list") {
	property(name="msg" field.delimiter="58" field.number="2")
	constant(value="\n")
}

if prifilt("local4.!=debug") then {
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}

if prifilt("local4.!=info;local4.!=debug") then {
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.compound.out.log" template="outfmt")
}

*.info;mail.none action(type="omfile" file="'$RSYSLOG_DYNNAME'.wildcard.out.log" template="outfmt")

if prifilt("mail,local5.=info") then {
	action(type="omfile" file="'$RSYSLOG_DYNNAME'.named.out.log" template="outfmt")
}
'
startup
tcpflood -m1 -P 165 -i0
tcpflood -m1 -P 166 -i1
tcpflood -m1 -P 167 -i2
tcpflood -m1 -P 22 -i3
tcpflood -m1 -P 173 -i4
tcpflood -m1 -P 174 -i5
shutdown_when_empty
wait_shutdown
content_check "00000000"
content_check "00000001"
check_not_present "00000002"
export RSYSLOG_OUT_LOG="$RSYSLOG_DYNNAME.compound.out.log"
content_check "00000000"
check_not_present "00000001"
check_not_present "00000002"
export RSYSLOG_OUT_LOG="$RSYSLOG_DYNNAME.wildcard.out.log"
content_check "00000000"
content_check "00000001"
content_check "00000004"
content_check "00000005"
check_not_present "00000002"
check_not_present "00000003"
export RSYSLOG_OUT_LOG="$RSYSLOG_DYNNAME.named.out.log"
content_check "00000003"
content_check "00000005"
check_not_present "00000000"
check_not_present "00000001"
check_not_present "00000002"
check_not_present "00000004"
exit_test
