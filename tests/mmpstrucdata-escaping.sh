#!/bin/bash
# This file is part of the rsyslog project, released  under ASL 2.0
# rgerhards, 2019-10-07
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/mmpstrucdata/.libs/mmpstrucdata")

template(name="outfmt" type="string" string="%$!rfc5424-sd%\n")

action(type="mmpstrucdata")
if $msg contains "TESTMESSAGE" then
	action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal '<85>1 2019-08-27T13:02:58.000+01:00 A/B-896747 ABC LMBNI SUCCESS [origin software="ABC" swVersion="47.1"][ABC@32473 eventType="XYZ:IPIP,9:\"free -m\";" remoteIp="192.0.2.1" singleTick="D'"'"'E" bracket="1\]2"] TESTMESSAGE'
shutdown_when_empty
wait_shutdown
export EXPECTED='{ "origin": { "software": "ABC", "swversion": "47.1" }, "abc@32473": { "eventtype": "XYZ:IPIP,9:\"free -m\";", "remoteip": "192.0.2.1", "singletick": "D'"'"'E", "bracket": "1]2" } }'
cmp_exact
exit_test
