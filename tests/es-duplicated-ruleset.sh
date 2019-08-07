#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=1500 # slow test, thus low number - large number is NOT necessary
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
download_elasticsearch
prepare_elasticsearch
start_elasticsearch

generate_conf
add_conf '
template(name="tpl" type="string" string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
ruleset(name="try_es") {
		action(type="omelasticsearch"
				 server="localhost" 
				 serverport=`echo $ES_PORT`
				 template="tpl"
				 searchIndex="rsyslog_testbench"
				 retryruleset="try_es"
				 )
}

ruleset(name="try_es") {
		action(type="omelasticsearch"
				 server="localhost" 
				 serverport=`echo $ES_PORT`
				 template="tpl"
				 searchIndex="rsyslog_testbench"
				 retryruleset="try_es"
				 )
}
'
startup
shutdown_immediate
wait_shutdown 
cleanup_elasticsearch
exit_test
