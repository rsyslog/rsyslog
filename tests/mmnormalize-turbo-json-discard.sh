#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# A discard match "%.:json%" gives every captured key a name that begins with
# the discard dot, so an input key of "." yields an all-dot field name. The
# turbo materializer split such names on "." and handed the leading token to
# json_object_object_add(), which is NULL for an all-dot name: strdup(NULL)
# aborts the worker. The field must instead be added under its literal name.
#
# Oracle: rsyslogd survives the dot-keyed record and every message is emitted.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin mmnormalize

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/mmnormalize/.libs/mmnormalize")
input(type="imtcp" port="0"
	listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="norm")

template(name="num" type="string" string="%$!num%\n")

ruleset(name="norm") {
	action(type="mmnormalize" useRawMsg="on" turbo="on" rule="rule=:%.:json%")
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="num")
}
'
startup
# the dot-keyed record is the one that used to abort the worker; the plain
# records on either side prove the instance is still processing afterwards
tcpflood -m1 -M "\"{\\\"num\\\":0}\""
tcpflood -m1 -M "\"{\\\".\\\":7,\\\"num\\\":1}\""
tcpflood -m1 -M "\"{\\\"..\\\":8,\\\"num\\\":2}\""
tcpflood -m1 -M "\"{\\\"num\\\":3}\""
shutdown_when_empty
wait_shutdown
seq_check 0 3
exit_test
