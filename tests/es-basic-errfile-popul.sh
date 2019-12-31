#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
export ES_PORT=19200
export NUMMESSAGES=1000 # slow test, thus low number - large number is NOT necessary
ensure_elasticsearch_ready

init_elasticsearch
curl -H 'Content-Type: application/json' -XPUT localhost:19200/rsyslog_testbench/ -d '{
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
generate_conf
add_conf '
# Note: we must mess up with the template, because we can not
# instruct ES to put further constraints on the data type and
# values. So we require integer and make sure it is none.
template(name="tpl" type="string"
	 string="{\"msgnum\":\"x%msg:F,58:2%\"}")


module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
				 template="tpl"
				 searchIndex="rsyslog_testbench"
				 searchType="test-type"
				 serverport="19200"
				 bulkmode="off"
				 errorFile="./'${RSYSLOG_DYNNAME}'.errorfile")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
if [ ! -f ${RSYSLOG_DYNNAME}.errorfile ]
then
    echo "error: error file does not exist!"
    error_exit 1
fi
exit_test
