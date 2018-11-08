#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
export NUMMESSAGES=1000 #10000

download_elasticsearch
stop_elasticsearch
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
	       serverport="19200"
	       template="tpl"
	       searchIndex="rsyslog_testbench")
'
startup
injectmsg  0 $NUMMESSAGES
shutdown_when_empty
wait_shutdown 

es_getdata $NUMMESSAGES 19200
stop_elasticsearch
seq_check  0 $(( NUMMESSAGES - 1 ))
cleanup_elasticsearch
exit_test
