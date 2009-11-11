# This tests two rsyslog instances. Instance
# TWO sends data to instance ONE. A number of messages is injected into
# the instance 2 and we finally check if all those messages
# arrived at instance 1.
# added 2009-11-11 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv.sh\]: testing sending and receiving via tcp
source $srcdir/sndrcv_drvr.sh sndrcv 50000
