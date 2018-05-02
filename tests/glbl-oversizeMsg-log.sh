#!/bin/bash
# add 2018-05-03 by PascalWithopf, released under ASL 2.0
. $srcdir/diag.sh init
./have_relpSrvSetOversizeMode
if [ $? -eq 1 ]; then
  echo "imrelp parameter oversizeMode not available. Test stopped"
  exit 77
fi;
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imrelp/.libs/imrelp")
global(maxMessageSize="230"
	oversizemsg.errorfile="rsyslog2.out.log")


input(type="imrelp" port="13514" maxdatasize="300")

template(name="outfmt" type="string" string="%rawmsg%\n")
action(type="omfile" template="outfmt"
				 file="rsyslog.out.log")
'
# TODO: add tcpflood option to specific EXACT test message size!
. $srcdir/diag.sh startup
. $srcdir/diag.sh tcpflood -Trelp-plain -p13514 -m1 -d 240
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown

grep "rawmsg.*<167>Mar  1 01:00:00 172.20.245.8 tag msgnum:00000000:240:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX.*input.*imrelp" rsyslog2.out.log > /dev/null
if [ $? -ne 0 ]; then
        echo
        echo "FAIL: expected message not found. rsyslog2.out.log is:"
        cat rsyslog2.out.log
        . $srcdir/diag.sh error-exit 1
fi


. $srcdir/diag.sh exit
