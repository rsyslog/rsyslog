#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../contrib/mmrfc5424addhmac/.libs/mmrfc5424addhmac")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

template(name="rawfmt" type="string" string="%rawmsg%\n")

action(type="mmrfc5424addhmac" key="secret" hashFunction="sha256" sd_id="hmac@32473")
if $msg contains "LONGSDIDTEST" then
	action(type="omfile" template="rawfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
LONG_SDID=$(head -c 256 < /dev/zero | tr '\0' 'A')
echo "<85>1 2026-05-08T00:00:00.000Z host app - - [${LONG_SDID} k=\"v\"] LONGSDIDTEST" > "$RSYSLOG_DYNNAME.input"
tcpflood -B -I "$RSYSLOG_DYNNAME.input"
rm -f "$RSYSLOG_DYNNAME.input"
shutdown_when_empty
wait_shutdown

content_check "LONGSDIDTEST"
exit_test
