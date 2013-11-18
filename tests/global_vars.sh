# Test for global variables
# added 2013-11-18 by rgerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[global_vars.sh\]: testing global variable support
source $srcdir/diag.sh init
source $srcdir/diag.sh startup global_vars.conf

# 40000 messages should be enough
source $srcdir/diag.sh injectmsg  0 40000

source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh seq-check 0 39999
source $srcdir/diag.sh exit
