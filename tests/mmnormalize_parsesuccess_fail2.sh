#!/bin/bash
# add 2018-05-11 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
input(type="imtcp" port="13514" ruleset="tcpinput")

template(name="outfmt" type="string" string="data: %$!data%, parsesuccess: %$!parsesuccess%\n")

ruleset(name="tcpinput") {
	action(name="main_cee_parser" type="mmnormalize" useRawMsg="off"
		rulebase="testsuites/mmnormalize_parsesuccess_fail2.rulebase")
	set $!parsesuccess = $parsesuccess;
	action(type="omfile" template="outfmt" file="rsyslog.out.log")	
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m2 -M "\"message without json\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo 'data: , parsesuccess: FAIL
data: , parsesuccess: FAIL' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
