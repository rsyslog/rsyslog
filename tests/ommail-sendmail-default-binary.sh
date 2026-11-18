#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# This file is part of the rsyslog project, released under ASL 2.0

# Verify that ommail uses its default sendmail binary when sendmail.binary is
# omitted. This invokes the host MTA and can deliver mail, so normal test runs
# skip it. The ordinary ommail-sendmail.sh test covers sendmail mode with an
# isolated helper; set RSYSLOG_TEST_OMMAIL_DEFAULT_SENDMAIL=1 to opt in here.

if [ "${TEST_RUN_TYPE:-}" != "MOCK-OK" ] && \
    [ "${RSYSLOG_TEST_OMMAIL_DEFAULT_SENDMAIL:-}" != "1" ]; then
    opt_in="RSYSLOG_TEST_OMMAIL_DEFAULT_SENDMAIL=1"
    echo "ommail-sendmail-default-binary: skipped; set $opt_in to invoke the host MTA"
    exit 77
fi

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
