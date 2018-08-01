#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test checks that omprog restarts the external program when it
# terminates prematurely, and that it does so without leaking file
# descriptors. Two cases are checked: termination of the program when
# omprog is going to write to the pipe (to send a message to the
# program), and when omprog is going to read from the pipe (when it
# is expecting the program to confirm the last message).

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary=`echo $srcdir/testsuites/omprog-restart-terminated-bin.sh`
        template="outfmt"
        name="omprog_action"
        queue.type="Direct"  # the default; facilitates sync with the child process
        confirmMessages="on"  # facilitates sync with the child process
        action.resumeRetryCount="10"
        action.resumeInterval="1"
        action.reportSuspensionContinuation="on"
        signalOnClose="off"
    )
}
'
. $srcdir/diag.sh check-command-available lsof

startup
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg 0 1
. $srcdir/diag.sh wait-queueempty

. $srcdir/diag.sh getpid
start_fd_count=$(lsof -p $pid | wc -l)

. $srcdir/diag.sh injectmsg 1 1
. $srcdir/diag.sh injectmsg 2 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 3 1
. $srcdir/diag.sh injectmsg 4 1
. $srcdir/diag.sh wait-queueempty

pkill -TERM -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 5 1
. $srcdir/diag.sh injectmsg 6 1
. $srcdir/diag.sh injectmsg 7 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 8 1
. $srcdir/diag.sh injectmsg 9 1
. $srcdir/diag.sh wait-queueempty

end_fd_count=$(lsof -p $pid | wc -l)
child_pid=$(ps -ef | grep "[o]mprog-restart-terminated-bin.sh" | awk '{print $2}')
child_netstat=$(netstat -p | grep "$child_pid/bash")

shutdown_when_empty
wait_shutdown

expected_output="Starting
Received msgnum:00000000:
Received msgnum:00000001:
Received msgnum:00000002:
Received SIGUSR1, will terminate after the next message
Received msgnum:00000003:
Terminating without confirming the last message
Starting
Received msgnum:00000003:
Received msgnum:00000004:
Received SIGTERM, terminating
Starting
Received msgnum:00000005:
Received msgnum:00000006:
Received msgnum:00000007:
Received SIGUSR1, will terminate after the next message
Received msgnum:00000008:
Terminating without confirming the last message
Starting
Received msgnum:00000008:
Received msgnum:00000009:
Terminating normally"

written_output=$(<$RSYSLOG_OUT_LOG)
if [[ "$expected_output" != "$written_output" ]]; then
    echo unexpected omprog script output:
    echo "$written_output"
    error_exit 1
fi

if [[ "$start_fd_count" != "$end_fd_count" ]]; then
    echo "file descriptor leak: started with $start_fd_count open files, ended with $end_fd_count"
    error_exit 1
fi

# Check also that the child process does not inherit open fds from
# rsyslog (apart from the pipe), by checking it has no open sockets.
# During the test, rsyslog has at least one socket open, by imdiag
# (port 13500).
if [[ "$child_netstat" != "" ]]; then
    echo "child process has socket(s) open (should have none):"
    echo "$child_netstat"
    error_exit 1
fi

exit_test
