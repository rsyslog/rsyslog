#!/bin/bash
# addd 2016-05-13 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 128
global(processInternalMessages="on")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="ruleset1" discardTruncatedMsg="on")

template(name="outfmt" type="string" string="%rawmsg%\n")
ruleset(name="ruleset1") {
	action(type="omfile" template="outfmt" file=`echo $RSYSLOG_OUT_LOG`)
}
'
startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 test15 test16\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<120> 2011-03-01T11:22:12Z host tag: this is a way to long message\""
shutdown_when_empty
wait_shutdown

echo '<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 t
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message that has abcdefghijklmnopqrstuvwxyz test1 test2 test3 test4 t
<120> 2011-03-01T11:22:12Z host tag: this is a way to long message' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat $RSYSLOG_OUT_LOG
  error_exit  1
fi;

exit_test
