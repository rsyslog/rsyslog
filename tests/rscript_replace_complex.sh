# added 2014-10-31 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[rscript_replace_complex.sh\]: a more complex test for replace script-function
source $srcdir/diag.sh init
source $srcdir/diag.sh startup rscript_replace_complex.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/complex_replace_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check "try to replace rsyslog and syrsyslog with rrsyslog"
source $srcdir/diag.sh content-check "try to replace hello_world in hello_worldlo and helhello_world with hello_world_world"
source $srcdir/diag.sh content-check "try to FBB in FBB_quux and quux_FBB with FBB"
source $srcdir/diag.sh exit
