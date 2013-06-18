# Test for the pipe output action.
# will create a fifo in the current directory, write to it and
# then do the usual sequence checks.
# added 2009-11-05 by RGerhards
echo ===============================================================================
echo \[pipeaction.sh\]: testing pipe output action

# create the pipe and start a background process that copies data from 
# it to the "regular" work file
source $srcdir/diag.sh init
rm -f rsyslog-testbench-fifo
mkfifo rsyslog-testbench-fifo
cp rsyslog-testbench-fifo rsyslog.out.log &
CPPROCESS=$!
echo background cp process id is $CPPROCESS

# now do the usual run
source $srcdir/diag.sh startup pipeaction.conf
# 20000 messages should be enough
#source $srcdir/diag.sh tcpflood -m20000
source $srcdir/diag.sh injectmsg 0 20000
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown

# wait for the cp process to finish, do pipe-specific cleanup
echo waiting for background cp to terminate...
wait $CPPROCESS
rm -f rsyslog-testbench-fifo
echo background cp has terminated, continue test...

# and continue the usual checks
source $srcdir/diag.sh seq-check 0 19999
source $srcdir/diag.sh exit
