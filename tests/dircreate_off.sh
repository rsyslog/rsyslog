# Test for automatic creation of dynafile directories
# note that we use the "test-spool" directory, because it is handled by diag.sh
# in any case, so we do not need to add any extra new test dir.
# added 2009-11-30 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===================================================================================
echo \[dircreate_off_off.sh\]: testing automatic directory creation for dynafiles - default
source $srcdir/diag.sh init
source $srcdir/diag.sh startup dircreate_off.conf
source $srcdir/diag.sh injectmsg  0 1 # a single message is sufficient
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
if [ -e test-logdir/rsyslog.out.log ]
then
	echo "test-logdir or logfile WAS created where not permitted to!"
	exit 1
fi
exit
source $srcdir/diag.sh exit
