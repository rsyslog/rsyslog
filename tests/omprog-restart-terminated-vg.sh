#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Same test than 'omprog-restart-terminated.sh', but checking for memory
# problems using valgrind. Note it is not necessary to repeat the
# rest of checks (this simplifies the maintenance of the tests).
. $srcdir/diag.sh init

uname -a
if [ `uname` = "SunOS" ] ; then
   echo "This test currently does not work on all flavors of Solaris"
   echo "looks like a problem with signal delivery to the script"
   exit 77
fi

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
startup_vg
wait_startup
injectmsg 0 1
. $srcdir/diag.sh wait-queueempty

injectmsg 1 1
injectmsg 2 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep 1 # ensure signal is delivered on (very) slow machines

injectmsg 3 1
injectmsg 4 1
. $srcdir/diag.sh wait-queueempty

pkill -TERM -f omprog-restart-terminated-bin.sh
sleep 1 # ensure signal is delivered on (very) slow machines

injectmsg 5 1
injectmsg 6 1
injectmsg 7 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep 1 # ensure signal is delivered on (very) slow machines

injectmsg 8 1
injectmsg 9 1
. $srcdir/diag.sh wait-queueempty

shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
