#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test tests the feedback feature of omprog (confirmMessages=on),
# by checking that omprog re-sends to the external program the messages
# it has failed to process.

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary=`echo $srcdir/testsuites/omprog-feedback-bin.sh`
        template="outfmt"
        name="omprog_action"
        queue.type="Direct"  # the default; facilitates sync with the child process
        confirmMessages="on"
        useTransactions="off"
        action.resumeRetryCount="10"
        action.resumeInterval="1"
    )
}
'
startup
. $srcdir/diag.sh wait-startup
injectmsg 0 10
. $srcdir/diag.sh wait-queueempty
shutdown_when_empty
wait_shutdown

expected_output="<= OK
=> msgnum:00000000:
<= OK
=> msgnum:00000001:
<= OK
=> msgnum:00000002:
<= OK
=> msgnum:00000003:
<= OK
=> msgnum:00000004:
<= Error: could not process log message
=> msgnum:00000004:
<= Error: could not process log message
=> msgnum:00000004:
<= OK
=> msgnum:00000005:
<= OK
=> msgnum:00000006:
<= OK
=> msgnum:00000007:
<= Error: could not process log message
=> msgnum:00000007:
<= Error: could not process log message
=> msgnum:00000007:
<= OK
=> msgnum:00000008:
<= OK
=> msgnum:00000009:
<= OK"

written_output=$(<$RSYSLOG_OUT_LOG)
if [[ "$expected_output" != "$written_output" ]]; then
    echo unexpected omprog script output:
    echo "$written_output"
    error_exit 1
fi

exit_test
