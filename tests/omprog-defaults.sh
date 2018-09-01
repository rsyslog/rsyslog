#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test tests omprog using the default values for all non-mandatory
# settings. It also checks that command-line parameters are correctly
# passed to the external program.

# NOTE: Because the omprog feedback mode is not used in this test
# (confirmMessages=off), it is difficult to synchronize the execution
# of the external program with the test code. For this reason, it would
# be difficult to test for negative cases (e.g. restart of the program
# if it terminates prematurely) without making the test racy. So, we
# only test a happy case: the program processes all logs received and
# exits when the pipe is closed. After closing the pipe, omprog will
# wait for the program to terminate during a timeout of 5 seconds
# (default value of closeTimeout), which should be sufficient for the
# program to write its output.

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
	binary=`echo $srcdir/testsuites/omprog-defaults-bin.sh param1 param2 param3`
        template="outfmt"
        name="omprog_action"
    )
}
'
startup
wait_startup
injectmsg 0 10
. $srcdir/diag.sh wait-queueempty
shutdown_when_empty
wait_shutdown

expected_output="Starting with parameters: param1 param2 param3
Received msgnum:00000000:
Received msgnum:00000001:
Received msgnum:00000002:
Received msgnum:00000003:
Received msgnum:00000004:
Received msgnum:00000005:
Received msgnum:00000006:
Received msgnum:00000007:
Received msgnum:00000008:
Received msgnum:00000009:
Terminating normally"

written_output=$(<$RSYSLOG_OUT_LOG)
if [[ "$expected_output" != "$written_output" ]]; then
    echo unexpected omprog script output:
    echo "$written_output"
    error_exit 1
fi

exit_test
