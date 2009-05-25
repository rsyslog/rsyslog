rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/manytcp.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
# the config file specifies exactly 1100 connections
./tcpflood 127.0.0.1 13514 1000 40000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 39999
if [ "$?" -ne "0" ]; then
  rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
