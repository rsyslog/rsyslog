# added 2011-02-28 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[imtcp-tls-basic.sh\]: testing imtcp in TLS mode - basic test
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imtcp-tls-basic.conf
source $srcdir/diag.sh tcpflood -p13514 -m50000 -Ttls -Z./tls-certs/cert.pem -z./tls-certs/key.pem
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 0 49999
source $srcdir/diag.sh exit
