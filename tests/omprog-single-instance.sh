#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test tests the omprog 'forceSingleInstance' flag by checking
# that only one instance of the program is started when multiple
# workers are in effect.

. ${srcdir:=.}/diag.sh init
skip_platform "SunOS"  "This test currently does not work on Solaris "
export NUMMESSAGES=10000  # number of logs to send
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary="'$srcdir'/testsuites/omprog-single-instance-bin.sh"
        template="outfmt"
        name="omprog_action"
        confirmMessages="on"
        forceSingleInstance="on"
        queue.type="LinkedList"  # use a dedicated queue
        queue.workerThreads="10"  # ...with multiple workers
        queue.size="5000"  # ...high capacity (default is 1000)
        queue.timeoutShutdown="30000"  # ...and a long shutdown timeout
    )
}
'
startup
injectmsg
shutdown_when_empty
wait_shutdown

EXPECTED_LINE_LENGTH=25    # example line: 'Received msgnum:00009880:'
line_num=0
while IFS= read -r line; do
    ((line_num++))
    if (( line_num == 1 )); then
        if [[ "$line" != "Starting" ]]; then
            echo "unexpected first line in output: $line"
            error_exit 1
        fi
    elif (( line_num == NUMMESSAGES + 2 )); then
        if [[ "$line" != "Terminating" ]]; then
            echo "unexpected last line in output: $line"
            error_exit 1
        fi
    elif [[ ${#line} != $EXPECTED_LINE_LENGTH ]]; then
        echo "unexpected line in output (line $line_num): $line"
        error_exit 1
    fi
done < $RSYSLOG_OUT_LOG

if (( line_num != NUMMESSAGES + 2 )); then
    echo "unexpected line count in output: $line_num (expected: $((NUMMESSAGES + 2)))"
    error_exit 1
fi

exit_test
