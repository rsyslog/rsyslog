#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=100
export ES_PORT=19200
ensure_elasticsearch_ready

init_elasticsearch
generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
                                 template="tpl"
                                 serverport=`echo $ES_PORT`
                                 searchType="_doc"
                                 searchIndex="rsyslog_testbench"
                                 bulkmode="on")
'
startup_vg
injectmsg
wait_queueempty
shutdown_when_empty
wait_shutdown_vg
es_getdata $NUMMESSAGES $ES_PORT
seq_check
exit_test
