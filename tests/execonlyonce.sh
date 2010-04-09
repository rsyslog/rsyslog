# Test for the $ActionExecOnlyOnceEveryInterval directive.
# We inject a couple of messages quickly during the interval,
# then wait until the interval expires, then quickly inject
# another set. After that, it is checked if exactly two messages
# have arrived.
# The once interval must be set to 3 seconds in the config file.
# added 2009-11-12 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[execonlyonce.sh\]: test for the $ActionExecOnlyOnceEveryInterval directive
source $srcdir/diag.sh init
source $srcdir/diag.sh startup execonlyonce.conf
source $srcdir/diag.sh tcpflood -m10 -i1
# now wait until the interval definitely expires
sleep 4 # one more than the once inerval!
# and inject another couple of messages
source $srcdir/diag.sh tcpflood -m10 -i100
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown

# now we need your custom logic to see if the result is equal to the
# expected result
cmp rsyslog.out.log testsuites/execonlyonce.data
if [ $? -eq 1 ]
then
	echo "ERROR, output not as expected"
	exit 1
fi
source $srcdir/diag.sh exit
