#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released  under GPLv3
echo ====================================================================================
echo TEST: \[imtcp_conndrop_tls.sh\]: test imtcp/tls with random connection drops
. $srcdir/diag.sh init
generate_conf
add_conf '
$MaxMessageSize 10k

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000

# TLS Stuff - certificate files - just CA for a client
$DefaultNetstreamDriver gtls
$IncludeConfig rsyslog.conf.tlscert
$InputTCPServerStreamDriverMode 1
$InputTCPServerStreamDriverAuthMode anon
$InputTCPServerRun 13514

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
$template dynfile,"rsyslog.out.log" # trick to use relative path names!
$OMFileFlushOnTXEnd off
$OMFileFlushInterval 2
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
echo \$DefaultNetstreamDriverCAFile $srcdir/tls-certs/ca.pem     >rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverCertFile $srcdir/tls-certs/cert.pem >>rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverKeyFile $srcdir/tls-certs/key.pem   >>rsyslog.conf.tlscert
startup
# 100 byte messages to gain more practical data use
. $srcdir/diag.sh tcpflood -c20 -p13514 -m50000 -r -d100 -P129 -D -l0.995 -Ttls -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
sleep 5 # due to large messages, we need this time for the tcp receiver to settle...
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
seq_check 0 49999 -E
# . $srcdir/diag.sh content-check 'XXXXX'	# Not really a check if it worked, but in TLS stuff in unfished TLS Packets gets lost, so we can't use seq-check.
exit_test
