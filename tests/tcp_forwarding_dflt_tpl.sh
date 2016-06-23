#!/bin/bash
# This test tests tcp forwarding with assigned default template.
# added 2015-05-30 by rgerhards. Released under ASL 2.0
echo ======================================================================================
echo \[tcp_forwarding_dflt_tpl.sh\]: test for tcp forwarding with assigned default template

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
. $srcdir/diag.sh init
./minitcpsrv -t127.0.0.1 -p13514 -frsyslog.out.log &
BGPROCESS=$!
echo background minitcpsrv process id is $BGPROCESS

# now do the usual run
. $srcdir/diag.sh startup tcp_forwarding_dflt_tpl.conf
# 10000 messages should be enough
. $srcdir/diag.sh injectmsg 0 10000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown

# note: minitcpsrv shuts down automatically if the connection is closed!
# (we still leave the code here in in case we need it later)
#echo shutting down minitcpsrv...
#kill $BGPROCESS
#wait $BGPROCESS
#echo background process has terminated, continue test...

# and continue the usual checks
. $srcdir/diag.sh seq-check 0 9999
. $srcdir/diag.sh exit
