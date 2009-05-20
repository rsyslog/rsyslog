# Test for Linkedlist queue mode
# added 2009-05-20 by rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo testing queue Linkedlist queue mode
rm -f work rsyslog.out.log
# enable this, if you need debug output: export RSYSLOG_DEBUG="debug"
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/arrayqueue.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
# 40000 messages should be enough
./tcpflood 127.0.0.1 13514 1 40000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
sleep 4 # we need this so that rsyslogd can receive all outstanding messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 39999
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
