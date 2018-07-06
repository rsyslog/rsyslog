#!/bin/bash
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" port="13514" ruleset="ruleset1")

template(name="outfmt" type="string" string="%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -T "udp" -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: before HT	after HT (do NOT remove TAB!)\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo ' before HT#011after HT (do NOT remove TAB!)' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
