#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514" ruleset="customparser")
parser(name="custom.rfc3164" type="pmrfc3164" force.tagEndingByColon="on")
template(name="outfmt" type="string" string="-%syslogtag%-%msg%-\n")

ruleset(name="customparser" parser="custom.rfc3164") {
	:syslogtag, contains, "tag" action(type="omfile" template="outfmt" file="rsyslog.out.log")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname1 tag1: msgnum:1\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname2 tag2:  msgnum:2\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname3 tag3 msgnum:3\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname4 tag4 :\""
. $srcdir/diag.sh tcpflood -m1 -M "\"<129>Mar 10 01:00:00 Hostname5 tag5:msgnum:5\""
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
echo '-tag1:- msgnum:1-
-tag2:-  msgnum:2-
-tag5:-msgnum:5-' | cmp rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;

. $srcdir/diag.sh exit
