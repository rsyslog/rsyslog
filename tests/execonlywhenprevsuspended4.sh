#!/bin/bash
# we test the execonly if previous is suspended directive.
# This test checks if multiple backup actions can be defined.
# rgerhards, 2010-06-24
echo ===============================================================================
echo \[execonlywhenprevsuspended4.sh\]: test execonly..suspended multi backup action
. $srcdir/diag.sh init
. $srcdir/diag.sh startup execonlywhenprevsuspended4.conf
. $srcdir/diag.sh injectmsg 0 1000
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 1 999
if [[ -s rsyslog2.out.log ]] ; then
   echo failure: second output file has data where it should be empty
   exit 1
fi ;
. $srcdir/diag.sh exit
