#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="remote" multiline="on")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -B -I testsuites/imptcp_multi_line.testdata
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
echo 'NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test1
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test2
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012line1
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012l#012i#012n#012#012e2
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test3
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag multi#012line3
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test4
NEWMSG: <133>Mar  1 01:00:00 172.20.245.8 tag test end' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;
. $srcdir/diag.sh exit
