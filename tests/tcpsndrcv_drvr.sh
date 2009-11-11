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
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
source $srcdir/diag.sh init
# start up the instances
source $srcdir/diag.sh startup $1_sender.conf 2
source $srcdir/diag.sh startup $1_rcvr.conf 
source $srcdir/diag.sh wait-startup2
source $srcdir/diag.sh wait-startup

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
source $srcdir/diag.sh tcpflood 127.0.0.1 13515 1 $2 1
# shut down sender when everything is sent, receiver continues to run concurrently
source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2
# now it is time to stop the receiver as well
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown

# do the final check
source $srcdir/diag.sh seq-check 1 $2
source $srcdir/diag.sh exit
