#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
$EscapeControlCharactersOnReceive off
global(maxMessageSize="256")
module(load="../plugins/imptcp/.libs/imptcp")
input(type="imptcp" port="13514" ruleset="remote"
	framing.delimiter.regex="^<[0-9]{2}>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)")

template(name="outfmt" type="string" string="NEWMSG: %rawmsg%\n")
ruleset(name="remote") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
action(type="omfile" file="rsyslog2.out.log")
'
startup
. $srcdir/diag.sh tcpflood -B -I ${srcdir}/testsuites/imptcp_framing_regex-oversize.testdata
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
echo 'NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test1
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test xml-ish
<test/>
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test2
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
line1
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
 1-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 2-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 3-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
NEWMSG: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 7-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 8-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
END
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test3
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag multi
line3
NEWMSG: <33>Mar  1 01:00:00 172.20.245.8 tag test4' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  error_exit  1
fi;
. $srcdir/diag.sh content-check-regex "assuming end of frame" rsyslog2.out.log
. $srcdir/diag.sh content-check-regex "message too long" rsyslog2.out.log
exit_test
