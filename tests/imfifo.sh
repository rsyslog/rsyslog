#!/bin/bash
# Test for the imfifo input module.
# Creates a POSIX named pipe, configures imfifo to read it,
# writes log messages, and verifies correctness.
# Added 2026-05-24, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# Verify imfifo is built and available
require_plugin imfifo

generate_conf
add_conf '
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
(
  echo "message 1" > $RSYSLOG_DYNNAME.fifo
  echo "message 2" > $RSYSLOG_DYNNAME.fifo
  echo "message 3" > $RSYSLOG_DYNNAME.fifo
) &
PIPE_WRITER_PID=$!

# Wait a moment for messages to be processed
./msleep 1000

# Initiate clean shutdown
shutdown_when_empty
wait_shutdown

# Clean up background process and FIFO file
wait $PIPE_WRITER_PID
rm -f $RSYSLOG_DYNNAME.fifo

# Assert correct output logging
echo "local4.notice pro: message 1
local4.notice pro: message 2
local4.notice pro: message 3" | cmp - $RSYSLOG_OUT_LOG
if [ ! $? -eq 0 ]; then
  echo "invalid response generated, $RSYSLOG_OUT_LOG is:"
  cat $RSYSLOG_OUT_LOG
  error_exit 1
fi;

exit_test
