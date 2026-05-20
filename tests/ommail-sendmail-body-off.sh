#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="mailBody" type="string" string="body must not be present %msg%\\r\\n")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="sendmail"
        sendmail.binary="'$srcdir'/testsuites/ommail-sendmail-bin.sh"
        mailfrom="rsyslog@example.net"
        mailto="operator@example.net"
        subject.text="body disabled"
        body.enable="off"
        template="mailBody"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check "From: <rsyslog@example.net>"
content_check "To: <operator@example.net>"
content_check "Subject: body disabled"
if grep -F -- "body must not be present" "$RSYSLOG_OUT_LOG" >/dev/null; then
    error_exit 1
fi

exit_test
