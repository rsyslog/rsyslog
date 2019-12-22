#!/bin/bash
# added 2015-11-17 by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0

uname
if [ $(uname) = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ===============================================================================
echo \[json_null_array.sh\]: test for json containing \"null\" value
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="outfmt" type="string" string="%$.data%\n")

action(type="mmjsonparse")
foreach ($.data in $!array) do {
	if not ($.data == "") then
		action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
}
'
startup_vg
tcpflood -m 1 -M "\"<167>Mar  6 16:57:54 172.20.245.8 test: @cee: { \\\"array\\\": [0, 1, null, 2, 3, null, 4] }\""
echo doing shutdown
shutdown_when_empty
echo wait on shutdown
wait_shutdown_vg
seq_check 0 4
exit_test
