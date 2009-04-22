# Test for DA mode on the main message queue
# This test checks if DA mode operates correctly. To do so,
# it uses a small in-memory queue size, so that DA mode is initiated
# rather soon, and disk spooling used. There is some uncertainty (based
# on machine speeds), but in general the test should work rather well.
# We add a few messages after the initial run, just so that we can
# check everything recovers from DA mode correctly.
# added 2009-04-22 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo "THIS TEST DOES NOT YET WORK RELIABLY!"
echo "testing main message queue in DA mode (going to disk)"
rm -f work rsyslog.out.log
rm -rf test-spool
mkdir test-spool
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/da-mainmsg-q.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
#
# part1: send first 50 messages (in memory, only)
#
./tcpflood 127.0.0.1 13514 1 50
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
ls -l test-spool
sleep 1 # we need this so that rsyslogd can receive all outstanding messages
#
# part 2: send bunch of messages. This should trigger DA mode
#
# 20000 messages should be enough - the disk test is slow enough ;)
./tcpflood 127.0.0.1 13514 2 20000 50
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
ls -l test-spool
sleep 5 # we need this so that rsyslogd can receive all outstanding messages
#
# send another handful
#
ls -l test-spool
./tcpflood 127.0.0.1 13514 1 50 20050
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
sleep 1 # we need this so that rsyslogd can receive all outstanding messages
# 
# clean up and check test result
#
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 20099
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
rm -rf test-spool
