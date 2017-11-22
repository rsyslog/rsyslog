#!/bin/bash
# add 2016-11-22 by Jan Gerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="outfmt" type="string" string="%msg%\n")

module(load="../plugins/mmanon/.libs/mmanon")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="testing")

ruleset(name="testing") {
	action(type="mmanon" ipv4.enable="on" ipv4.mode="zero" ipv4.bits="32" ipv6.bits="128" ipv6.anonmode="zero" ipv6.enable="on")
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}'

. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 172.20.245.8 tag: FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF
<129>Mar 10 01:00:00 172.20.245.8 tag: 1.1.1.8 space 61:34:ad::7:F
<129>Mar 10 01:00:00 172.20.245.8 tag: 111.1.1.8
<129>Mar 10 01:00:00 172.20.245.8 tag: abf:3:002::500F:ce 1.1.1.9\""

. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo ' 0:0:0:0:0:0:0:0
 0.0.0.0 space 0:0:0:0:0:0:0:0
 0.0.0.0
 0:0:0:0:0:0:0:0 0.0.0.0' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
