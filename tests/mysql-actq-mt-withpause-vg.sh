#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[mysql-act-mt.sh\]: test for mysql with multithread actionq
. $srcdir/diag.sh init
mysql --user=rsyslog --password=testbench < testsuites/mysql-truncate.sql
startup_vg mysql-actq-mt.conf
. $srcdir/diag.sh injectmsg  0 50000
. $srcdir/diag.sh wait-queueempty 
echo waiting for worker threads to timeout
./msleep 3000
. $srcdir/diag.sh injectmsg  50000 50000
. $srcdir/diag.sh wait-queueempty 
echo waiting for worker threads to timeout
./msleep 2000
. $srcdir/diag.sh injectmsg  100000 50000
shutdown_when_empty
wait_shutdown_vg 
. $srcdir/diag.sh check-exit-vg
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < testsuites/mysql-select-msg.sql > rsyslog.out.log
seq_check  0 149999
exit_test
