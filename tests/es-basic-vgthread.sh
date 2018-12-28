#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_PORT=19200
export NUMMESSAGES=5000 # test is pretty slow, so use a low number
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

if $msg contains "msgnum:" then {
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport=`echo $ES_PORT`
	       template="tpl"
	       searchIndex="rsyslog_testbench")
}
'
startup_vgthread
injectmsg
shutdown_when_empty
wait_shutdown_vg 
check_exit_vg
es_getdata
seq_check
cleanup_elasticsearch
exit_test
