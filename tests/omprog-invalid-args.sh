#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Regression coverage for GitHub issue 5017. The omprog child exits during
# initialization when it receives an invalid argument. Success is proven by
# synchronized shutdown plus rsyslog's own diagnostic in the configured omfile
# destination, not by rsyslogd stdout/stderr.

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary="'$srcdir'/testsuites/omprog-invalid-args-bin.sh --invalid"
        template="outfmt"
        name="omprog_action"
        queue.type="Direct"
        confirmMessages="on"
        action.resumeRetryCount="0"
    )
}

syslog.* {
    action(type="omfile" template="outfmt" file=`echo $RSYSLOG2_OUT_LOG`)
}
'

startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown

expected_prog="${srcdir}/testsuites/omprog-invalid-args-bin.sh"
content_check "omprog: program '$expected_prog' " "$RSYSLOG2_OUT_LOG"
content_check "terminated; will be restarted" "$RSYSLOG2_OUT_LOG"
custom_assert_content_missing "Segmentation fault" "$RSYSLOG2_OUT_LOG"

exit_test
