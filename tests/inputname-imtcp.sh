#!/bin/bash
# add 2018-06-29 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")

input(type="imtcp" name="12514" port="12514" ruleset="ruleset1")
input(type="imtcp" name="12515" port="12515" ruleset="ruleset1")
input(type="imtcp" name="12516" port="12516" ruleset="ruleset1")

template(name="outfmt" type="string" string="%inputname%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -p12514 -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
. $srcdir/diag.sh tcpflood -p12515 -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
. $srcdir/diag.sh tcpflood -p12516 -m1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 %PIX-7-710005: MSG\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '12514
12515
12516' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
