#!/bin/bash
# added 2026-08-07 by Julien Thomas, released under ASL 2.0
# Regression test: an mmexternal program that returns a core property set to a
# JSON null ({"msg": null}) must NOT crash rsyslogd. jsonToString() returns NULL
# for a JSON null and the string setters in msgSetPropViaJSON() (msg, rawmsg,
# syslogtag, hostname, ...) used to strlen(NULL) -> SIGSEGV. The dispatch is
# shared, so mmlua and pmnormalize hit the same path. A null core property is
# now ignored (it can be neither null nor unset), leaving the value unchanged.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")
template(name="outfmt" type="string" string="msg=%msg%\n")
action(type="mmexternal" binary="'$srcdir'/testsuites/mmexternal-null-prop-bin.sh msg")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:test-message"
shutdown_when_empty
wait_shutdown

# no crash, and the null value left "msg" untouched
content_check "msg=test-message"
exit_test
