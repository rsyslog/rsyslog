# This tests basic omuxsock functionality. A socket receiver is started which sends
# all data to an output file, then a rsyslog instance is started which generates
# messages and sends them to the unix socket. Datagram sockets are being used.
# added 2010-08-06 by Rgerhards
echo ===============================================================================
echo \[uxsock_simple.sh\]: simple tests for omuxsock functionality

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
source $srcdir/diag.sh init
./uxsockrcvr -srsyslog-testbench-dgram-uxsock -orsyslog.out.log &
BGPROCESS=$!
echo background uxsockrcvr process id is $BGPROCESS

# now do the usual run
source $srcdir/diag.sh startup uxsock_simple.conf
# 10000 messages should be enough
source $srcdir/diag.sh injectmsg 0 10000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo shutting down uxsockrcvr...
# TODO: we should do this more reliable in the long run! (message counter? timeout?)
kill $BGPROCESS
wait $BGPROCESS
echo background process has terminated, continue test...

# and continue the usual checks
source $srcdir/diag.sh seq-check 0 9999
source $srcdir/diag.sh exit
