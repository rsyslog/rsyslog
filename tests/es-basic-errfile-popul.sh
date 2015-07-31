#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[es-basic-errfile-popul\]: basic test for elasticsearch functionality
. $srcdir/diag.sh init
. $srcdir/diag.sh es-init
curl -XPUT localhost:9200/rsyslog_testbench/ -d '{
  "mappings": {
    "test-type": {
      "properties": {
        "msgnum": {
          "type": "integer"
        }
      }
    }
  }
}'
. $srcdir/diag.sh startup es-basic-errfile-popul.conf
. $srcdir/diag.sh injectmsg  0 1000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
if [ ! -f rsyslog.errorfile ]
then
    echo "error: error file does not exist!"
    exit 1
fi
. $srcdir/diag.sh exit
