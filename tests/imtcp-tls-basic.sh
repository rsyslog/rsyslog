#!/bin/bash
# added 2011-02-28 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[imtcp-tls-basic.sh\]: testing imtcp in TLS mode - basic test
. $srcdir/diag.sh init
echo \$DefaultNetstreamDriverCAFile $srcdir/tls-certs/ca.pem     >rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverCertFile $srcdir/tls-certs/cert.pem >>rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverKeyFile $srcdir/tls-certs/key.pem   >>rsyslog.conf.tlscert
. $srcdir/diag.sh startup imtcp-tls-basic.conf
. $srcdir/diag.sh tcpflood -p13514 -m50000 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 0 49999
. $srcdir/diag.sh exit
