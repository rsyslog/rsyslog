#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imtcp_conndrop_tls.sh\]: test imtcp/tls with random connection drops
. $srcdir/diag.sh init
echo \$DefaultNetstreamDriverCAFile $srcdir/tls-certs/ca.pem     >rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverCertFile $srcdir/tls-certs/cert.pem >>rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverKeyFile $srcdir/tls-certs/key.pem   >>rsyslog.conf.tlscert
startup imtcp_conndrop_tls.conf
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -p13514 -m50000 -r -d100 -P129 -D -l0.995 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
sleep 5 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 49999 -E
# . $srcdir/diag.sh content-check 'XXXXX'	# Not really a check if it worked, but in TLS stuff in unfished TLS Packets gets lost, so we can't use seq-check.
exit_test
