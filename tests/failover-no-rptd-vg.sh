# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[failover-no-rptd.sh\]: rptd test for failover functionality - no failover
source $srcdir/diag.sh init
source $srcdir/diag.sh startup-vg failover-no-rptd.conf
source $srcdir/diag.sh injectmsg  0 5000
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown-vg
source $srcdir/diag.sh check-exit-vg
# now we need our custom logic to see if the result file is empty
# (what it should be!)
cmp rsyslog.out.log /dev/null
if [ $? -eq 1 ]
then
	echo "ERROR, output file not empty"
	exit 1
fi
source $srcdir/diag.sh exit
