#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Similar to the 'omprog-restart-terminated.sh' test, using the 'outfile'
# parameter. The use of this parameter has implications on the file
# descriptors handled by omprog.

. $srcdir/diag.sh init
. $srcdir/diag.sh check-command-available lsof

startup omprog-restart-terminated-outfile.conf
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

written_output=$(<rsyslog.out.log)
if [[ "$expected_output" != "$written_output" ]]; then
    echo unexpected omprog script output:
    echo "$written_output"
    error_exit 1
fi

if [[ "$start_fd_count" != "$end_fd_count" ]]; then
    echo "file descriptor leak: started with $start_fd_count open files, ended with $end_fd_count"
    error_exit 1
fi

exit_test
