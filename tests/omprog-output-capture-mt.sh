#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Similar to the 'omprog-output-capture.sh' test, with multiple worker
# threads on high load. Checks that the lines concurrently emitted to
# stdout/stderr by the various program instances are not intermingled in
# the output file (i.e., are captured atomically by omprog) when 1) the
# lines are less than PIPE_BUF bytes long and 2) the program writes the
# lines in unbuffered or line-buffered mode.

. ${srcdir:=.}/diag.sh init
skip_platform "SunOS" "This test currently does not work on all flavors of Solaris (problems with Python?)"
if [ "$CC" == "gcc" ] && [[ "$CFLAGS" == *"-coverage"* ]]; then
	printf 'This test does not work with gcc coverage instrumentation\n'
	printf 'It will hang, but we do not know why. See\n'
	printf 'https://github.com/rsyslog/rsyslog/issues/3361\n'
	exit 77
fi

export NUMMESSAGES=20000

if [[ "$(uname)" == "Linux" ]]; then
    export LINE_LENGTH=4095   # 4KB minus 1 byte (for the newline char)
else
    export LINE_LENGTH=511   # 512 minus 1 byte (for the newline char)
fi

empty_check() {
	if [ $(wc -l < "$RSYSLOG_OUT_LOG") -ge $((NUMMESSAGES * 2)) ]; then
		return 0
	fi
	return 1
}
export QUEUE_EMPTY_CHECK_FUNC=empty_check

generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

main_queue(
    queue.timeoutShutdown="60000"  # long shutdown timeout for the main queue
)

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary="'$PYTHON' '$srcdir'/testsuites/omprog-output-capture-mt-bin.py '$LINE_LENGTH'"
        template="outfmt"
        name="omprog_action"
        queue.type="LinkedList"  # use a dedicated queue
        queue.workerThreads="10"  # ...with many workers
        queue.timeoutShutdown="60000"  # ...and a long shutdown timeout
        closeTimeout="10000"  # wait enough for program to terminate
        output=`echo $RSYSLOG_OUT_LOG`
        fileCreateMode="0644"
    )
}
'
startup

# Issue a HUP signal when the output-capture thread has not started yet,
# to check that this case is handled correctly (the output file has not
# been opened yet, so omprog must not try to reopen it).
issue_HUP

injectmsg 0 $NUMMESSAGES

# Issue more HUP signals to cause the output file to be reopened during
# writing (not a complete test of this feature, but at least we check it
# doesn't break the output).
issue_HUP
./msleep 1000
issue_HUP
./msleep 1000
issue_HUP

#wait_file_lines "$RSYSLOG_OUT_LOG" $((NUMMESSAGES * 2))
shutdown_when_empty
wait_shutdown

line_num=0
while IFS= read -r line; do
    ((line_num++))
    if [[ ${#line} != $LINE_LENGTH ]]; then
        echo "intermingled line in captured output: line: $line_num, length: ${#line} (expected: $LINE_LENGTH)"
        echo "$line"
        error_exit 1
    fi
done < $RSYSLOG_OUT_LOG

if (( line_num != NUMMESSAGES * 2 )); then
    echo "unexpected number of lines in captured output: $line_num (expected: $((NUMMESSAGES * 2)))"
    error_exit 1
fi

exit_test
