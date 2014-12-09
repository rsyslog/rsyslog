# added 2014-11-03 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_tokenized.sh\]: test for mmnormalize tokenized field_type
source $srcdir/diag.sh init
source $srcdir/diag.sh startup mmnormalize_tokenized.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/tokenized_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
cp rsyslog.out.log /tmp/
source $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "90.100.110.120", "130.140.150.160" ]'
source $srcdir/diag.sh content-check '[ "192.168.1.2", "192.168.1.3", "192.168.1.4" ]'
source $srcdir/diag.sh content-check '[ "10.20.30.40", "50.60.70.80", "190.200.210.220" ]'
source $srcdir/diag.sh content-check '[ "\/bin", "\/usr\/local\/bin", "\/usr\/bin" ] foo'
source $srcdir/diag.sh content-check '[ [ [ "10" ] ], [ [ "20" ], [ "30", "40", "50" ], [ "60", "70", "80" ] ], [ [ "90" ], [ "100" ] ] ]'
source $srcdir/diag.sh exit
