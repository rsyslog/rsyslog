# test many concurrent tcp connections
echo \[manytcp-too-few-tls.sh\]: test concurrent tcp connections
source $srcdir/diag.sh init
source $srcdir/diag.sh startup-vg manytcp-too-few-tls.conf
echo wait for DH param generation -- NOT needed in v6!
sleep 15
# the config file specifies exactly 1100 connections
source $srcdir/diag.sh tcpflood -c1000 -m40000
# the sleep below is needed to prevent too-early termination of the tcp listener
sleep 1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown-vg	# we need to wait until rsyslogd is finished!
source $srcdir/diag.sh check-exit-vg
source $srcdir/diag.sh seq-check 0 39999
source $srcdir/diag.sh exit
