#!/bin/bash
# Test for the imfifo input module.
#
# Intent:
# This test verifies that the 'imfifo' input module correctly reads log messages
# from a POSIX named pipe (FIFO) and processes them line-by-line under dynamic
# configuration.
#
# Invariant Tested:
# 1. Dynamic module loading and input instance parsing.
# 2. Correct extraction of newline-delimited messages from a FIFO.
# 3. Message routing to standard rsyslog action queue with tags, facility, and severity.
# 4. A writer that keeps a line open past maxMessageSize cannot grow the
#    accumulator without bound; the overlong line is truncated and flushed at
#    the newline.
#
# The Oracle:
# Success is proven by writing three short messages and one overlong message to
# a FIFO in a background process, cleanly shutting down rsyslog, and asserting that the output file contains
# exactly the four messages with the expected tags, facility, and severity. The
# fourth message proves the size oracle by comparing the output with the first
# maxMessageSize bytes of a longer FIFO line.
#
# Timing / Wait Justification:
# wait_file_lines polls until all expected messages reached the configured
# output, avoiding fixed sleeps while still synchronizing before
# shutdown_when_empty.
#
# Added 2026-05-24, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# Verify imfifo is built and available
require_plugin imfifo

generate_conf
add_conf '
global(maxMessageSize="128")
module(load="../plugins/imfifo/.libs/imfifo")

input(type="imfifo"
      file="'$RSYSLOG_DYNNAME'.fifo"
      tag="pro:"
      severity="notice"
      facility="local4")

template(name="outfmt" type="string" string="%syslogfacility-text%.%syslogseverity-text% %syslogtag% %msg%\n")

if $syslogtag == "pro:" then action(type="omfile" File=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

# Create named pipe
rm -f $RSYSLOG_DYNNAME.fifo
mkfifo $RSYSLOG_DYNNAME.fifo

# Start rsyslogd
startup

# Send messages to FIFO in a background task
LONG_MSG="$(printf "%*s" 200 "" | tr " " "A")"
EXPECTED_LONG="${LONG_MSG:0:128}"
(
  echo "message 1" > $RSYSLOG_DYNNAME.fifo
  echo "message 2" > $RSYSLOG_DYNNAME.fifo
  echo "message 3" > $RSYSLOG_DYNNAME.fifo
  printf "%s\n" "$LONG_MSG" > $RSYSLOG_DYNNAME.fifo
) &
PIPE_WRITER_PID=$!

# Wait until all messages have been processed
wait_file_lines "$RSYSLOG_OUT_LOG" 4 60

# Initiate clean shutdown
shutdown_when_empty
wait_shutdown

# Clean up background process and FIFO file
wait $PIPE_WRITER_PID
rm -f $RSYSLOG_DYNNAME.fifo

# Assert correct output logging
echo "local4.notice pro: message 1
local4.notice pro: message 2
local4.notice pro: message 3
local4.notice pro: ${EXPECTED_LONG}" | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
