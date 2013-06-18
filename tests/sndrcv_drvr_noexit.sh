# This is test driver for testing two rsyslog instances. It can be
# utilized by any test that just needs two instances with different
# config files, where messages are injected in instance TWO and 
# (with whatever rsyslog mechanism) being relayed over to instance ONE,
# where they are written to the log file. After the run, the completeness
# of that log file is checked.
# The code is almost the same, but the config files differ (probably greatly)
# for different test cases. As such, this driver needs to be called with the
# config file name ($2). From that name, the sender and receiver config file
# names are automatically generated. 
# So: $1 config file name, $2 number of messages
#
# A note on TLS testing: the current testsuite (in git!) already contains
# TLS test cases. However, getting these test cases correct is not simple.
# That's not a problem with the code itself, but rater a problem with
# synchronization in the test environment. So I have deciced to keep the
# TLS tests in, but not yet actually utilize them. This is most probably
# left as an excercise for future (devel) releases. -- rgerhards, 2009-11-11
#
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
source $srcdir/diag.sh init
# start up the instances
#export RSYSLOG_DEBUG="debug nostdout noprintmutexaction"
#export RSYSLOG_DEBUGLOG="log"
source $srcdir/diag.sh startup $1_rcvr.conf 
source $srcdir/diag.sh wait-startup
#export RSYSLOG_DEBUGLOG="log2"
#valgrind="valgrind"
source $srcdir/diag.sh startup $1_sender.conf 2
source $srcdir/diag.sh wait-startup 2
# may be needed by TLS (once we do it): sleep 30

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
source $srcdir/diag.sh tcpflood -m$2 -i1
sleep 2 # make sure all data is received in input buffers
# shut down sender when everything is sent, receiver continues to run concurrently
# may be needed by TLS (once we do it): sleep 60
source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2
# now it is time to stop the receiver as well
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown

# may be needed by TLS (once we do it): sleep 60
# do the final check
source $srcdir/diag.sh seq-check 1 $2
