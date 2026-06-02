#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
#
# Intent: exercise the sendmail execv() failure path after the configured
# binary passes the executable preflight. A script with a missing interpreter
# is executable, but execv() fails with ENOENT.

. ${srcdir:=.}/diag.sh init
generate_conf

BAD_SENDMAIL="$RSYSLOG_DYNNAME.bad-sendmail"
{
	echo '#!/does/not/exist'
	echo 'exit 0'
} > "$BAD_SENDMAIL"
chmod 755 "$BAD_SENDMAIL"

add_conf '
module(load="../plugins/ommail/.libs/ommail")

template(name="outfmt" type="string" string="%msg%\n")
template(name="mailBody" type="string" string="%msg%\\r\\n")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $msg contains "msgnum:" then {
    action(
        type="ommail"
        mode="sendmail"
        sendmail.binary="'$BAD_SENDMAIL'"
        mailfrom="rsyslog@example.net"
        mailto="operator@example.net"
        subject.text="exec failure"
        template="mailBody"
        action.resumeRetryCount="0"
    )
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

content_check "ommail: failed to execute sendmail binary"
content_check "No such file or directory"

exit_test
