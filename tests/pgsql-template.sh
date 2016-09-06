#!/bin/bash
# This file is part of the rsyslog project, released under GPLv3
echo ===============================================================================
echo \[postgres-basic.sh\]: testing template support for the postgres output
. $srcdir/diag.sh init
psql -h db -U postgres -d Syslog -f testsuites/pgsql-truncate.sql

. $srcdir/diag.sh startup pgsql-template.conf
. $srcdir/diag.sh injectmsg  0 5000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 

# we actually put the message in the SysLogTag field, so we know it doesn't use the default
# template, like in pgsql-basic
psql -h db -U postgres -d Syslog -f testsuites/pgsql-select-syslogtag.sql -t -A > rsyslog.out.log 

. $srcdir/diag.sh seq-check  0 4999
. $srcdir/diag.sh exit
