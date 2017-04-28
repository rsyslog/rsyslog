#!/bin/bash
# add 2016-11-22 by Pascal Withopf, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" File="testsuites/NotExistingInputFile" Tag="tag1" fileNotFoundError="off")

action(type="omfile" file="rsyslog.out.log")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "imfile: error.*No such file or directory" rsyslog.out.log > /dev/null
if [ $? -eq 0 ]; then
        echo
        echo "FAIL: error message from missing input file found. rsyslog.out.log is:"
        cat rsyslog.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
