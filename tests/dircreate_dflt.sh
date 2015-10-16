#!/bin/bash
# Test for automatic creation of dynafile directories
# note that we use the "test-spool" directory, because it is handled by diag.sh
# in any case, so we do not need to add any extra new test dir.
# added 2009-11-30 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
# uncomment for debugging support:
echo ===================================================================================
echo \[dircreate_dflt_dflt.sh\]: testing automatic directory creation for dynafiles - default
. $srcdir/diag.sh init
. $srcdir/diag.sh startup dircreate_dflt.conf
. $srcdir/diag.sh injectmsg  0 1 # a single message is sufficient
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
if [ ! -e test-logdir/rsyslog.out.log ]
then
	echo "test-logdir or logfile not created!"
	exit 1
fi
exit
. $srcdir/diag.sh exit
