#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset1")

global(localHostname="localhost")

template(name="outfmt" type="string" string="%PRI%,%syslogfacility-text%,%syslogseverity-text%,%timestamp%,%programname%,%syslogtag%,%msg%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<175>Oct 16 23:47:31 #001 MSWinEventLog 0#011Security#01119023582#011Fri Oct 16 16:30:44 2009#011592#011Security#011rgabcde#011User#011Success Audit#011XSXSXSN01#011Detailed Tracking#011#0112572#01119013885\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '175,local5,debug,Oct 16 23:47:31,#001,#001, MSWinEventLog 0#011Security#01119023582#011Fri Oct 16 16:30:44 2009#011592#011Security#011rgabcde#011User#011Success Audit#011XSXSXSN01#011Detailed Tracking#011#0112572#01119013885' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
