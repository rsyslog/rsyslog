# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[elasticsearch-bulk-errorfile-populated\]: basic test for elasticsearch functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh es-init
echo '{ "name" : "foo" }
{"name": bar"}
{"name": "baz"}
{"name": foz"}' > inESData.inputfile
source $srcdir/diag.sh startup elasticsearch-bulk-errorfile-populated-default-format.conf
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown
rm -f inESData.inputfile

python $srcdir/elasticsearch-error-format-check.py default

if [ $? -ne 0 ]
then
    echo "error: Format for error file different! " $?
    exit 1
fi
source $srcdir/diag.sh exit
