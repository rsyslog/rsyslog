#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="mailBody" type="string" string="alert %msg%\\r\\n.second line\\r\\n")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="sendmail"
        sendmail.binary="'$srcdir'/testsuites/ommail-sendmail-bin.sh"
        mailfrom="rsyslog@example.net"
        mailto=["operator@example.net", "admin@example.net"]
        subject.text="rsyslog alert"
        body.enable="on"
        template="mailBody"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check "ARGV_BEGIN"
content_check "ARG:-i"
content_check "ARG:-f"
content_check "ARG:rsyslog@example.net"
content_check "ARG:--"
content_check "ARG:operator@example.net"
content_check "ARG:admin@example.net"
content_check "STDIN_BEGIN"
content_check "Date: "
content_check "From: <rsyslog@example.net>"
content_check "To: <admin@example.net>, <operator@example.net>"
content_check "Subject: rsyslog alert"
content_check "X-Mailer: rsyslog-ommail"
content_check "alert  msgnum:00000000:"
content_check ".second line"
content_check "STDIN_END"

exit_test
