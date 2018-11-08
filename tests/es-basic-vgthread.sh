#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
export ES_PORT=19200
download_elasticsearch
stop_elasticsearch
prepare_elasticsearch
. $srcdir/diag.sh start-elasticsearch

. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh es-init
generate_conf
add_conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")

if $msg contains "msgnum:" then
	action(type="omelasticsearch"
	       server="127.0.0.1"
	       serverport=`echo $ES_PORT`
	       template="tpl"
	       searchIndex="rsyslog_testbench")
'
startup_vgthread
injectmsg  0 10000
shutdown_when_empty
wait_shutdown_vg 
check_exit_vg
es_getdata 10000 $ES_PORT
seq_check  0 9999
stop_elasticsearch
. $srcdir/diag.sh cleanup-elasticsearch
exit_test
