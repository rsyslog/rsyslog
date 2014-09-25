# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[elasticsearch-basic-errorfile-populated\]: basic test for elasticsearch functionality
source $srcdir/diag.sh init
source $srcdir/diag.sh es-init
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
source $srcdir/diag.sh startup elasticsearch-basic-errorfile-populated.conf
source $srcdir/diag.sh injectmsg  0 1000
source $srcdir/diag.sh shutdown-when-empty
source $srcdir/diag.sh wait-shutdown 
if [ ! -f rsyslog.errorfile ]
then
    echo "error: error file does not exist!"
    exit 1
fi
source $srcdir/diag.sh exit
