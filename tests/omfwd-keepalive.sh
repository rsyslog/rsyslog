#!/bin/bash
# addd 2016-03-30 by RGerhards, released under ASL 2.0

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

template(name="outfmt" type="list") {
	property(name="msg")
	constant(value="\n")
}
:msg, contains, "x-pid" stop

action(type="omfile" template="outfmt" file="rsyslog.out.log")

:msg, contains, "this does not occur" action(type="omfwd"
	target="10.0.0.1" keepalive="on" keepalive.probes="10"
	keepalive.time="60" keepalive.interval="10")
 
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo " msgnum:00000000:" | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid output generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
