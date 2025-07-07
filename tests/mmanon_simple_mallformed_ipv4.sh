#!/bin/bash
# add 2022-07-28 by Andre Lorbach, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
#export USE_VALGRIND="YES" # this test only makes sense with valgrind enabled
#export RS_TEST_VALGRIND_EXTRA_OPTS="--keep-debuginfo=yes"

#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="$RSYSLOG_DYNNAME.debuglog"

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.bits="32" ipv4.mode="simple")
	action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}'

startup
tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: 165874883373.1.15599155266856607338.91@whatever
<129>Mar 10 01:00:00 172.20.245.8 tag: 1.165874883373.15599155266856607338.91@whatever
<129>Mar 10 01:00:00 172.20.245.8 tag: 15599155266856607338.165874883373.1.91@whatever
<129>Mar 10 01:00:00 172.20.245.8 tag: 91.165874883373.1.15599155266856607338.@whatever\""

shutdown_when_empty
wait_shutdown
export EXPECTED=' 165874883373.1.15599155266856607338.91@whatever
 1.165874883373.15599155266856607338.91@whatever
 15599155266856607338.165874883373.1.91@whatever
 91.165874883373.1.15599155266856607338.@whatever'
cmp_exact
exit_test
