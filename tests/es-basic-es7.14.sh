#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_DOWNLOAD=elasticsearch-7.14.1-linux-x86_64.tar.gz
export ES_PORT=19200
export NUMMESSAGES=2000 # slow test
export QUEUE_EMPTY_CHECK_FUNC=es_shutdown_empty_check
ensure_elasticsearch_ready
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
injectmsg
shutdown_when_empty
wait_shutdown 
es_getdata
seq_check
exit_test
