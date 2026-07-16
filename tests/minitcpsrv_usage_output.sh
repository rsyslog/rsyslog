#!/bin/bash
# Verify that minitcpsrv's failure usage includes required addressing and the
# bounded receive-buffer option used by deterministic backpressure tests. The
# oracle is the exact option text in the diagnostic output; no timing is used.
# added 2018-11-15 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

./minitcpsrv&> $RSYSLOG_DYNNAME.output
grep -q -- "-t parameter missing" $RSYSLOG_DYNNAME.output 

if [ ! $? -eq 0 ]; then
  echo "invalid response generated"
  error_exit  1
fi;

grep -q -- "-b receiveBufferBytes" $RSYSLOG_DYNNAME.output || error_exit 1

exit_test
