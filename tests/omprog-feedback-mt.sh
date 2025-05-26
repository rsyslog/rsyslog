#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Similar to the 'omprog-feedback.sh' test, with multiple worker threads
# on high load, and a given error rate (percentage of failed messages, i.e.
# confirmed as failed by the program). Note: the action retry interval
# (1 second) causes a very low throughput; we need to set a very low error
# rate to avoid the test lasting too much.
. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "On Solaris, this test causes rsyslog to hang for unknown reasons"
if [ "$CC" == "gcc" ] && [[ "$CFLAGS" == *"-coverage"* ]]; then
	printf 'This test does not work with gcc coverage instrumentation\n'
	printf 'It will hang, but we do not know why. See\n'
	printf 'https://github.com/rsyslog/rsyslog/issues/3361\n'
	exit 77
fi

NUMMESSAGES=10000       # number of logs to send
ERROR_RATE_PERCENT=1    # percentage of logs to be retried

export command_line="$srcdir/testsuites/omprog-feedback-mt-bin.sh $ERROR_RATE_PERCENT"

generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

main_queue(
    queue.timeoutShutdown="30000"  # long shutdown timeout for the main queue
)

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary=`echo $command_line`
        template="outfmt"
        name="omprog_action"
        confirmMessages="on"
        queue.type="LinkedList"  # use a dedicated queue
        queue.workerThreads="10"  # ...with multiple workers
        queue.size="10000"  # ...high capacity (default is 1000)
        queue.timeoutShutdown="60000"  # ...and a long shutdown timeout
        action.resumeInterval="1"  # retry interval: 1 second
    )
}
'
startup
injectmsg 0 $NUMMESSAGES
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" $NUMMESSAGES
shutdown_when_empty
wait_shutdown
exit_test
