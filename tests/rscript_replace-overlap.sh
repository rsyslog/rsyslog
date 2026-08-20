#!/bin/bash
# Regression test for replace() sizing after a failed partial match overlaps a
# later match. The exact output values prove both replacement semantics and
# that the shared three-argument wrap() path writes the complete result.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string"
         string="long=%$.long%;short=%$.short%;equal=%$.equal%;tail=%$.tail%;empty=%$.empty%;wrap=%$.wrapped%\n")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" ruleset="replace_regression"
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

ruleset(name="replace_regression") {
set $.long = replace($msg, "ab", "0123456789");
set $.short = replace("aab", "ab", "X");
set $.equal = replace("ababa", "ab", "XY");
set $.tail = replace("aab", "abc", "Z");
set $.empty = replace("ab", "", "Z");
set $.wrapped = wrap("aab", "ab", "0123456789");

action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup
tcpflood -m1 -M '"<13>Aug 20 00:00:00 host tag: aab"'
shutdown_when_empty
wait_shutdown
content_check 'long= a0123456789;short=aX;equal=XYXYa;tail=aab;empty=Z;wrap=aba0123456789ab'
exit_test
