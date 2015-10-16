#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[es-bulk-errfile-popul-def-interleaved\]: basic test for elasticsearch functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh es-init
echo '{ "name" : "foo" }
{"name": bar"}
{"name": "baz"}
{"name": foz"}' > inESData.inputfile
. $srcdir/diag.sh startup es-bulk-errfile-popul-def-interleaved.conf
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown
rm -f inESData.inputfile

python $srcdir/elasticsearch-error-format-check.py interleaved

if [ $? -ne 0 ]
then
    echo "error: Format for error file different! " $?
    exit 1
fi
. $srcdir/diag.sh exit
