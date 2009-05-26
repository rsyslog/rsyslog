# Test for memory queue which is persisted at shutdown. The
# plan is to start an instance, emit some data, do a relatively
# fast shutdown and then re-start the engine to process the 
# remaining data.
# added 2009-05-25 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
#set -o xtrace
#export RSYSLOG_DEBUG="debug nostdout"
#export RSYSLOG_DEBUGLOG="log"
echo testing memory queue persisting to disk
$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
rm -f core.*
rm -rf test-spool
mkdir test-spool
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
#valgrind ../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/memq-persist1.conf &
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/memq-persist1.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
# 20000 messages should be enough
./tcpflood 127.0.0.1 13514 1 10000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
sleep 4 # we need to wait to ensure everything is received (less 1 second would be better)
kill `cat rsyslog.pid`
echo wait for shutdown
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
echo There must exist some files now:
ls -l test-spool
# restart engine and have rest processed
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/memq-persist2.conf &
sleep 1
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq -fwork -e9999 -d
if [ "$?" -ne "0" ]; then
  rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
rm -rf test-spool
