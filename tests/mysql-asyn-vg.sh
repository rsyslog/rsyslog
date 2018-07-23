#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[mysql-asyn.sh\]: asyn test for mysql functionality
. $srcdir/diag.sh init
mysql --user=rsyslog --password=testbench < testsuites/mysql-truncate.sql
startup_vg mysql-asyn.conf
. $srcdir/diag.sh injectmsg  0 50000
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < testsuites/mysql-select-msg.sql > rsyslog.out.log
seq_check  0 49999
exit_test
