#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[postgres-basic.sh\]: basic test for postgres output functionality
. $srcdir/diag.sh init
psql -h db -U postgres -d Syslog -f testsuites/pgsql-truncate.sql

. $srcdir/diag.sh startup pgsql-basic.conf
. $srcdir/diag.sh injectmsg  0 5000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 

psql -h db -U postgres -d Syslog -f testsuites/pgsql-select-msg.sql -t -A > rsyslog.out.log 

. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
