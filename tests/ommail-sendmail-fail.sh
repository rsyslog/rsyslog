#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
export OMMAIL_SENDMAIL_EXIT=42
generate_conf
add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="outfmt" type="string" string="%msg%\n")
template(name="mailBody" type="string" string="%msg%\\r\\n")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="sendmail"
        sendmail.binary="'$srcdir'/testsuites/ommail-sendmail-bin.sh"
        mailfrom="rsyslog@example.net"
        mailto="operator@example.net"
        subject.text="failure"
        template="mailBody"
        action.resumeRetryCount="0"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check "ommail: sendmail binary"
content_check "exited with status 42"

exit_test
