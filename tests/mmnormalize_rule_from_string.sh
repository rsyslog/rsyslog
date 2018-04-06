#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")

input(type="imtcp" port="13514" ruleset="norm")

template(name="outfmt" type="string" string="%hostname% %syslogtag%\n")

ruleset(name="norm") {
	action(type="mmnormalize" useRawMsg="on" rule="rule=:%host:word% %tag:char-to:\\x3a%: no longer listening on %ip:ipv4%#%port:number%")
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"ubuntu tag1: no longer listening on 127.168.0.1#10514\""
. $srcdir/diag.sh tcpflood -m1 -M "\"debian tag2: no longer listening on 127.168.0.2#10514\""
. $srcdir/diag.sh tcpflood -m1 -M "\"centos tag3: no longer listening on 192.168.0.1#10514\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo 'ubuntu tag1:
debian tag2:
centos tag3:' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
