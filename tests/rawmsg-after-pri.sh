#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
startup rawmsg-after-pri.conf
. $srcdir/diag.sh tcpflood -m1 -P 129
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
echo "Mar  1 01:00:00 172.20.245.8 tag msgnum:00000000:" > rsyslog.out.compare
NUMLINES=$(grep -c "^Mar  1 01:00:00 172.20.245.8 tag msgnum:00000000:$" rsyslog.out.log 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: output file seems not to exist"
  ls -l rsyslog.out.log
  cat rsyslog.out.log
  error_exit 1
else
  if [ ! $NUMLINES -eq 1 ]; then
    echo "ERROR: output format does not match expectation"
    cat rsyslog.out.log
    error_exit 1
  fi
fi
exit_test
