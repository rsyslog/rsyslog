#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[mysql-asyn.sh\]: asyn test for mysql functionality
. $srcdir/diag.sh init
mysql --user=rsyslog --password=testbench < testsuites/mysql-truncate.sql
. $srcdir/diag.sh startup mysql-asyn.conf
. $srcdir/diag.sh injectmsg  0 50000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < testsuites/mysql-select-msg.sql > rsyslog.out.log
. $srcdir/diag.sh seq-check  0 49999
. $srcdir/diag.sh exit
