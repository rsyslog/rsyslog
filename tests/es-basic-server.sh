#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=1500 # slow test, thus low number - large number is NOT necessary
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
ensure_elasticsearch_ready

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
                                 server="localhost"
                                 serverport=`echo $ES_PORT`
                                 template="tpl"
                                 searchType="_doc"
                                 searchIndex="rsyslog_testbench")
'
startup
injectmsg
shutdown_when_empty
wait_shutdown 
es_getdata
seq_check
exit_test
