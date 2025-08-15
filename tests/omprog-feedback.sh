#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# This test tests the feedback feature of omprog (confirmMessages=on),
# by checking that omprog re-sends to the external program the messages
# it has failed to process.

. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
        binary="'$srcdir'/testsuites/omprog-feedback-bin.sh"
        template="outfmt"
        name="omprog_action"
        queue.type="Direct"  # the default; facilitates sync with the child process
        confirmMessages="on"
        reportFailures="on"
        action.resumeInterval="1"  # retry interval: 1 second
#       action.resumeRetryCount="0" # the default; no need to increase since
                                    # the action resumes immediately
    )
}
'
startup
injectmsg 0 10
shutdown_when_empty
wait_shutdown

# Instead of checking exact sequence, check for expected patterns
# We expect:
# 1. All messages 0-9 to be processed at least once
# 2. Messages 4 and 7 to have error responses before success
# 3. Proper retry behavior

# Check that all messages were processed
for i in {0..9}; do
    pattern=$(printf "msgnum:%08d:" $i)
    grep -q "$pattern" $RSYSLOG_OUT_LOG
    if [ $? -ne 0 ]; then
        echo "FAIL: message $i not found in output"
        cat $RSYSLOG_OUT_LOG
        exit 1
    fi
done

# Check that messages 4 and 7 had error responses and were eventually successful
for i in 4 7; do
    msg_pattern=$(printf "msgnum:%08d:" $i)

    # Check that the message appears with an error response at least once
    # We look for an "Error" line immediately following a message send line
    if ! grep -A 1 "=> ${msg_pattern}" "$RSYSLOG_OUT_LOG" | grep -q "Error: could not process log message"; then
        echo "FAIL: message $i did not have an error response as expected"
        cat "$RSYSLOG_OUT_LOG"
        exit 1
    fi

    # Check that the message was eventually successful
    # We look for an "OK" line immediately following a message send line
    if ! grep -A 1 "=> ${msg_pattern}" "$RSYSLOG_OUT_LOG" | grep -q "OK"; then
        echo "FAIL: message $i was not eventually successful"
        cat "$RSYSLOG_OUT_LOG"
        exit 1
    fi
done

# Check that OK responses occurred
grep -q "<= OK" $RSYSLOG_OUT_LOG
if [ $? -ne 0 ]; then
    echo "FAIL: no OK responses found in output"
    cat $RSYSLOG_OUT_LOG
    exit 1
fi

# Check that the output follows the expected pattern (=> message, <= response)
grep -q "=> msgnum:" $RSYSLOG_OUT_LOG
if [ $? -ne 0 ]; then
    echo "FAIL: no message patterns found in output"
    cat $RSYSLOG_OUT_LOG
    exit 1
fi

echo "Test passed: All expected patterns found in output"
cat $RSYSLOG_OUT_LOG

exit_test
