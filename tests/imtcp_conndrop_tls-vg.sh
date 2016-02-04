#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imtcp_conndrop_tls-vg.sh\]: test imtcp/tls with random connection drops
. $srcdir/diag.sh init
echo \$DefaultNetstreamDriverCAFile $srcdir/tls-certs/ca.pem     >rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverCertFile $srcdir/tls-certs/cert.pem >>rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverKeyFile $srcdir/tls-certs/key.pem   >>rsyslog.conf.tlscert
. $srcdir/diag.sh startup-vg imtcp_conndrop_tls.conf
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -p13514 -m10000 -r -d100 -P129 -D -l0.995 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
sleep 5 # due to large messages, we need this time for the tcp receiver to settle...
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
. $srcdir/diag.sh seq-check 0 9999 -E
. $srcdir/diag.sh exit
