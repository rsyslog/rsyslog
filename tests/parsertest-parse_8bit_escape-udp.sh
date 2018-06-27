#!/bin/bash
# add 2018-06-28 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="13514" ruleset="ruleset1")

$Escape8BitCharactersOnReceive on

template(name="outfmt" type="string" string="%PRI%,%syslogfacility-text%,%syslogseverity-text%,%timestamp%,%hostname%,%programname%,%syslogtag%,%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -T "udp" -M "\"<6>AUG 10 22:18:24 host tag This msg contains 8-bit European chars: äöü\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '6,kern,info,Aug 10 22:18:24,host,tag,tag, This msg contains 8-bit European chars: #303#244#303#266#303#274' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
