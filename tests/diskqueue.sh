# Test for disk-only queue mode
# This test checks if queue files can be correctly written
# and read back, but it does not test the transition from
# memory to disk mode for DA queues.
# added 2009-04-17 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo testing queue disk-only mode
rm -rf test-spool
mkdir test-spool
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/diskqueue.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
# 20000 messages should be enough - the disk test is slow enough ;)
./tcpflood 127.0.0.1 13514 1 20000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
sleep 4 # we need this so that rsyslogd can receive all outstanding messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 19999
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
rm -rf test-spool
