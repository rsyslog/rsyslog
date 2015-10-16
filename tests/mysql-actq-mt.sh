#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mysql-act-mt.sh\]: test for mysql with multithread actionq
. $srcdir/diag.sh init
mysql --user=rsyslog --password=testbench < testsuites/mysql-truncate.sql
. $srcdir/diag.sh startup mysql-actq-mt.conf
. $srcdir/diag.sh injectmsg  0 150000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < testsuites/mysql-select-msg.sql > rsyslog.out.log
. $srcdir/diag.sh seq-check  0 149999
. $srcdir/diag.sh exit
