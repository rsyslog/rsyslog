#!/bin/bash
#check if rsyslog instance exists and, if so, kill it
echo initial ps
ps -ef|grep rsyslog
if [ -e "rsyslog.pid" ]
then
  echo rsyslog.pid exists, trying to shut down rsyslogd process `cat rsyslog.pid`.
  kill -9 `cat rsyslog.pid`
  sleep 1
  rm rsyslog.pid
fi
if [ -e "rsyslog2.pid" ]
then
  echo rsyslog2.pid exists, trying to shut down rsyslogd process `cat rsyslog2.pid`.
  kill -9 `cat rsyslog2.pid`
  sleep 1
  rm rsyslog2.pid
fi
echo terminal ps
ps -ef|grep rsyslog
