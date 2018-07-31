#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0

. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv6.bits="129" ipv6.anonmode="zero")
	action(type="omfile" file="./rsyslog.out.log" template="outfmt")
}
action(type="omfile" file=`echo $RSYSLOG2_OUT_LOG`)'

startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: asdfghjk
<129>Mar 10 01:00:00 172.20.245.8 tag: FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
<129>Mar 10 01:00:00 172.20.245.8 tag: 61:34:ad::7:F aa:ff43::756:99:0
<129>Mar 10 01:00:00 172.20.245.8 tag: ::
<129>Mar 10 01:00:00 172.20.245.8 tag: 0::
<129>Mar 10 01:00:00 172.20.245.8 tag: 13:abd:45:
<129>Mar 10 01:00:00 172.20.245.8 tag: textnoblank72:8374:adc7:47FF::43:0:1AFEstillnoblank\""

shutdown_when_empty
wait_shutdown
echo ' asdfghjk
 0:0:0:0:0:0:0:0
 0:0:0:0:0:0:0:0 0:0:0:0:0:0:0:0
 0:0:0:0:0:0:0:0
 0:0:0:0:0:0:0:0
 13:abd:45:
 textnoblank0:0:0:0:0:0:0:0stillnoblank' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit  1
fi;


grep 'invalid number of ipv6.bits (129), corrected to 128' rsyslog2.out.log > /dev/null
if [ $? -ne 0 ]; then
  echo "invalid correction of bits parameter generated, rsyslog2.out.log is:"
  cat rsyslog2.out.log
  error_exit  1
fi;

exit_test
