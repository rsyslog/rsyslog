#!/bin/bash
#check if rsyslog instance exists and, if so, kill it
if [ -e "rsyslog.pid" ]
then
  echo rsyslog.pid exists, trying to shut down rsyslogd process `cat rsyslog.pid`.
  fuser -k -KILL rsyslog.pid.lock
  sleep 1
  rm rsyslog.pid
fi
if [ -e "rsyslog2.pid" ]
then
  echo rsyslog2.pid exists, trying to shut down rsyslogd process `cat rsyslog2.pid`.
  fuser -k -KILL rsyslog2.pid.lock
  sleep 1
  rm rsyslog2.pid
fi
