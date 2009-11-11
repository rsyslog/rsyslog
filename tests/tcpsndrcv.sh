# This is the first-ever test for two rsyslog instances. It can
# probably be used as a copy-template for all those tests, and it
# is thus kept very simple.
# Besides that, it is useful. It tests two rsyslog instances. Instance
# TWO sends data to instance ONE. A number of messages is injected into
# the instance 2 and we finally check if all those messages
# arrived at instance 1.
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===============================================================================
echo \[tcpsndrcv.sh\]: testing sending and receiving via tcp
source $srcdir/diag.sh init
# start up the instances
source $srcdir/diag.sh startup tcpsndrcv_sender.conf 2
source $srcdir/diag.sh startup tcpsndrcv_rcvr.conf 
source $srcdir/diag.sh wait-startup2
source $srcdir/diag.sh wait-startup

# now inject the messages into instance 2. It will connect to instance 1,
# and that instance will record the data.
source $srcdir/diag.sh tcpflood 127.0.0.1 13515 1 50000
# shut down sender when everything is sent, receiver continues to run concurrently
source $srcdir/diag.sh shutdown-when-empty 2
source $srcdir/diag.sh wait-shutdown 2
# now it is time to stop the receiver as well
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown

# do the final check
source $srcdir/diag.sh seq-check 0 49999
source $srcdir/diag.sh exit
