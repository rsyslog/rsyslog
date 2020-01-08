#!/bin/bash
# Test imtcp/TLS with many dropping connections
# added 2011-06-09 by Rgerhards
#
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=50000
export QUEUE_EMPTY_CHECK_FUNC=wait_seq_check
generate_conf
add_conf '
$MaxMessageSize 10k

$ModLoad ../plugins/imtcp/.libs/imtcp
$MainMsgQueueTimeoutShutdown 10000

# TLS Stuff - certificate files - just CA for a client
$DefaultNetstreamDriver gtls
$IncludeConfig '$RSYSLOG_DYNNAME'.rsyslog.conf.tlscert
$InputTCPServerStreamDriverMode 1
$InputTCPServerStreamDriverAuthMode anon
$InputTCPServerRun '$TCPFLOOD_PORT'

$template outfmt,"%msg:F,58:2%,%msg:F,58:3%,%msg:F,58:4%\n"
template(name="dynfile" type="string" string=`echo $RSYSLOG_OUT_LOG`) # trick to use relative path names!
$OMFileIOBufferSize 256k
local0.* ?dynfile;outfmt
'
echo \$DefaultNetstreamDriverCAFile $srcdir/tls-certs/ca.pem     >$RSYSLOG_DYNNAME.rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverCertFile $srcdir/tls-certs/cert.pem >>$RSYSLOG_DYNNAME.rsyslog.conf.tlscert
echo \$DefaultNetstreamDriverKeyFile $srcdir/tls-certs/key.pem   >>$RSYSLOG_DYNNAME.rsyslog.conf.tlscert
startup
# 100 byte messages to gain more practical data use
tcpflood -c20 -p$TCPFLOOD_PORT -m$NUMMESSAGES -r -d100 -P129 -D -l0.995 -Ttls -x$srcdir/tls-certs/ca.pem -Z$srcdir/tls-certs/cert.pem -z$srcdir/tls-certs/key.pem
shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown       # and wait for it to terminate
export SEQ_CHECK_OPTIONS=-E
seq_check
exit_test
