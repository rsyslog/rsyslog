# test many concurrent tcp connections
source $srcdir/diag.sh init
source $srcdir/diag.sh startup manytcp.conf
# the config file specifies exactly 1100 connections
source $srcdir/diag.sh tcpflood 127.0.0.1 13514 1000 40000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh seq-check 0 39999
source $srcdir/diag.sh exit
