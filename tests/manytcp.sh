rm -f rsyslog.out.log # work file
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -ftestsuites/manytcp.conf &
echo "rsyslogd started with pid " `cat rsyslog.pid`
./tcpflood 127.0.0.1 13514 1000 20000
sleep 1
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 19999
if [ "$?" -ne "0" ]; then
  echo "sequence error detected"
  exit 1
fi
