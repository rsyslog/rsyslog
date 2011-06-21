# test many concurrent tcp connections
echo \[manytcp-too-few-tls.sh\]: test concurrent tcp connections
source $srcdir/diag.sh init
source $srcdir/diag.sh startup-vg manytcp-too-few-tls.conf
# the config file specifies exactly 1100 connections
source $srcdir/diag.sh tcpflood -c1000 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown-vg	# we need to wait until rsyslogd is finished!
source $srcdir/diag.sh check-exit-vg
# we do not do a seq check, as of the design of this test some messages
# will be lost. So there is no point in checking if all were received. The
# point is that we look at the valgrind result, to make sure we do not
# have a mem leak in those error cases (we had in the past, thus the test
# to prevent that in the future).
source $srcdir/diag.sh exit
