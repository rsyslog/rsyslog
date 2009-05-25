# Test for multiple ports in imtcp
# This test checks if multiple tcp listener ports are correctly
# handled by imtcp
#
# NOTE: this test must (and can) be enhanced when we merge in the
#       upgraded tcpflood program
#
# added 2009-05-22 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo testing imtcp multiple listeners
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/imtcp-multiport.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
./tcpflood 127.0.0.1 13514 1 10000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 9999
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
#
#
# ### now complete new cycle with other port ###
#
#
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/imtcp-multiport.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
./tcpflood 127.0.0.1 13515 1 10000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 9999
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
#
#
# ### now complete new cycle with other port ###
#
#
rm -f work rsyslog.out.log rsyslog.out.log.save # work files
../tools/rsyslogd -c4 -u2 -n -irsyslog.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/imtcp-multiport.conf &
sleep 1
echo "rsyslogd started with pid " `cat rsyslog.pid`
./tcpflood 127.0.0.1 13516 1 10000
if [ "$?" -ne "0" ]; then
  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
  cp rsyslog.out.log rsyslog.out.log.save
fi
$srcdir/waitqueueempty.sh # wait until rsyslogd is done processing messages
kill `cat rsyslog.pid`
rm -f work
sort < rsyslog.out.log > work
./chkseq work 0 9999
if [ "$?" -ne "0" ]; then
 # rm -f work rsyslog.out.log
  echo "sequence error detected"
  exit 1
fi
rm -f work rsyslog.out.log
