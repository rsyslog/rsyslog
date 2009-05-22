#check if rsyslog instance exists and, if so, kill it
if [ -e "rsyslog.pid" ]
then
  echo rsyslog.pid exists, trying to shut down rsyslogd process `cat rsyslog.pid`.
  kill `cat rsyslog.pid`
  sleep 1
fi
