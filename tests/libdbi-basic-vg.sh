#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
# this test is currently not included in the testbench as libdbi
# itself seems to have a memory leak
echo ===============================================================================
echo \[libdbi-basic.sh\]: basic test for libdbi-basic functionality via mysql
. $srcdir/diag.sh init
mysql --user=rsyslog --password=testbench < testsuites/mysql-truncate.sql
. $srcdir/diag.sh startup-vg-noleak libdbi-basic.conf
. $srcdir/diag.sh injectmsg  0 5000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
# note "-s" is requried to suppress the select "field header"
mysql -s --user=rsyslog --password=testbench < testsuites/mysql-select-msg.sql > rsyslog.out.log
. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
