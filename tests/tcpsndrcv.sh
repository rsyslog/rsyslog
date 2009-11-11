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
source $srcdir/tcpsndrcv_drvr.sh tcpsndrcv 50000
