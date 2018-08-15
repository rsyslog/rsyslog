#!/bin/bash
# This test is similar to tcpsndrcv, but it forwards messages in
# zlib-compressed format (our own syslog extension).
# rgerhards, 2009-11-11
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv_gzip.sh\]: testing sending and receiving via tcp in zlib mode
. $srcdir/diag.sh init
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
export RSYSLOG_DEBUGLOG="log"
generate_conf
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
# then SENDER sends to this port (not tcpflood!)
$InputTCPServerRun 13515

$template outfmt,"%msg:F,58:2%\n"
$template dynfile,"'$RSYSLOG_OUT_LOG'" # trick to use relative path names!
:msg, contains, "msgnum:" ?dynfile;outfmt
'
startup
. $srcdir/diag.sh wait-startup
export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
generate_conf 2
add_conf '
$ModLoad ../plugins/imtcp/.libs/imtcp
$InputTCPServerRun 13514

*.*	@@127.0.0.1:13515
' 2
startup 2
. $srcdir/diag.sh wait-startup 2
# may be needed by TLS (once we do it): sleep 30

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
tcpflood -m50000 -i1
sleep 5 # make sure all data is received in input buffers
# shut down sender when everything is sent, receiver continues to run concurrently
# may be needed by TLS (once we do it): sleep 60
shutdown_when_empty 2
wait_shutdown 2
# now it is time to stop the receiver as well
shutdown_when_empty
wait_shutdown

# may be needed by TLS (once we do it): sleep 60
# do the final check
seq_check 1 50000 $3
