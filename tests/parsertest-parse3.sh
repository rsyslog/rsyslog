#!/bin/bash
# add 2018-06-27 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="ruleset1")


template(name="outfmt" type="string" string="%timereported:1:19:date-rfc3339,csv%, %hostname:::csv%, %programname:::csv%, %syslogtag:R,ERE,0,BLANK:[0-9]+--end:csv%, %syslogseverity:::csv%, %msg:::drop-last-lf,csv%\n")

ruleset(name="ruleset1") {
	action(type="omfile" file="rsyslog.out.log"
	       template="outfmt")
}

'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<175>Oct 16 2009 23:47:31 hostname tag This is a message\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<175>Oct 16 2009 23:47:31 hostname tag[1234] This is a message\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

echo '"2009-10-16T23:47:31", "hostname", "tag", "", "7", " This is a message"
"2009-10-16T23:47:31", "hostname", "tag", "1234", "7", " This is a message"' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
