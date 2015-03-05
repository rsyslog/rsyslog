# added 2014-11-17 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mmnormalize_regex.sh\]: test for mmnormalize regex field_type
source $srcdir/diag.sh init
source $srcdir/diag.sh startup mmnormalize_regex.conf
source $srcdir/diag.sh tcpflood -m 1 -I $srcdir/testsuites/regex_input
echo doing shutdown
source $srcdir/diag.sh shutdown-when-empty
echo wait on shutdown
source $srcdir/diag.sh wait-shutdown 
source $srcdir/diag.sh content-check 'host and port list: 192.168.1.2:80, 192.168.1.3, 192.168.1.4:443, 192.168.1.5'
source $srcdir/diag.sh exit
