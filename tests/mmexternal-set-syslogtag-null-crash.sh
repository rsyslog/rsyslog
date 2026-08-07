#!/bin/bash
# added 2026-08-07 by Julien Thomas, released under ASL 2.0
# Same null-value regression as mmexternal-set-msg-null-crash.sh, but on
# "syslogtag" ({"syslogtag": null}) -- shows the crash was not specific to
# "msg": every string setter in msgSetPropViaJSON() that does
# strlen(jsonToString(json)) was affected. A null core property is ignored,
# leaving the value unchanged.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
module(load="../plugins/mmexternal/.libs/mmexternal")
template(name="outfmt" type="string" string="tag=%syslogtag%\n")
action(type="mmexternal" binary="'$srcdir'/testsuites/mmexternal-null-prop-bin.sh syslogtag")
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup
injectmsg literal "<13>Mar 10 01:00:00 host tag:test-message"
shutdown_when_empty
wait_shutdown

# no crash, and the null value left "syslogtag" untouched
content_check "tag=tag:"
exit_test
