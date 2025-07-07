#!/bin/bash
# add 2018-11-15 by Jan Gerhards, released under ASL 2.0

. ${srcdir:=.}/diag.sh init

./minitcpsrv&> $RSYSLOG_DYNNAME.output
grep -q -- "-t parameter missing" $RSYSLOG_DYNNAME.output 

if [ ! $? -eq 0 ]; then
  echo "invalid response generated"
  error_exit  1
fi;

exit_test
