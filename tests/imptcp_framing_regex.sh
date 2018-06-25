#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$EscapeControlCharactersOnReceive off
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="remote"
	framing.delimiter.regex="^<[0-9]{2}>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -B -I testsuites/imptcp_framing_regex.testdata
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
echo 'NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test1
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test xml-ish
<test/>
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test2
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
line1
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
 l
 i
 n

e2
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test3
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
line3
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test4' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit  1
fi;
. $srcdir/diag.sh exit
