# note: we must be root and no other syslogd running in order to
# carry out this test
echo \[imuxsock_traillf_root.sh\]: test trailing LF handling in imuxsock
echo This test must be run as root with no other active syslogd
if [ "$EUID" -ne 0 ]; then
    exit 77 # Not root, skip this test
fi
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imuxsock_traillf_root.conf
# send a message with trailing LF
./syslog_inject -l
# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
cmp rsyslog.out.log $srcdir/resultdata/imuxsock_traillf.log
if [ ! $? -eq 0 ]; then
echo "imuxsock_traillf_root.sh failed"
exit 1
fi;
source $srcdir/diag.sh exit
