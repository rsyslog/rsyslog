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

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
	    binary="'$srcdir'/testsuites/omprog-defaults-bin.sh \"p1 with spaces\"'\
' p2 \"\" --p4=\"middle quote\" \"--p6=\"proper middle quote\"\" \"p7 is last\""
        template="outfmt"
        name="omprog_action"
    )
}
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown

export EXPECTED="Starting with parameters: p1 with spaces p2  --p4=\"middle quote\" --p6=\"proper middle quote\" p7 is last
Next parameter is \"p1 with spaces\"
Next parameter is \"p2\"
Next parameter is \"\"
Next parameter is \"--p4=\"middle\"
Next parameter is \"quote\"\"
Next parameter is \"--p6=\"proper middle quote\"\"
Next parameter is \"p7 is last\"
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

cmp_exact $RSYSLOG_OUT_LOG

exit_test
