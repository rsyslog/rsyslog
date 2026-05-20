#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

if [ ! -x /usr/sbin/sendmail ]; then
    echo "ommail-sendmail-default-binary: /usr/sbin/sendmail not executable, skipping"
    skip_test
fi

generate_conf
add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="mailBody" type="string" string="default binary smoke %msg%\\r\\n")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="sendmail"
        mailfrom="rsyslog@example.net"
        mailto="operator@example.net"
        subject.text="default binary smoke"
        template="mailBody"
        action.resumeRetryCount="0"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

exit_test
