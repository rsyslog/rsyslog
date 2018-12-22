#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
export ES_PORT=19200
export NUMMESSAGES=1000 # 1000 is sufficient, as this test is pretty slow
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check

download_elasticsearch
prepare_elasticsearch
start_elasticsearch
init_elasticsearch

generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport="'$ES_PORT'"
	       template="tpl"
	       searchIndex="rsyslog_testbench")
'
startup
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown 

es_getdata $NUMMESSAGES $ES_PORT
seq_check  0 $(( NUMMESSAGES - 1 ))
cleanup_elasticsearch
exit_test
