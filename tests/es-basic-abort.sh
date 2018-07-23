#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
. $srcdir/diag.sh download-elasticsearch
. $srcdir/diag.sh stop-elasticsearch
. $srcdir/diag.sh prepare-elasticsearch
. $srcdir/diag.sh start-elasticsearch

#  Starting actual testbench
# TODO: move up,
. $srcdir/diag.sh init
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
		action.resumeRetryCount="-1"
		action.resumeInterval="1" # we need quick retries
		searchIndex="rsyslog_testbench")
'
startup
. $srcdir/diag.sh injectmsg  0 10000
./msleep 500
echo stop elasticsearch...
. $srcdir/diag.sh stop-elasticsearch
echo start elasticsearch...
. $srcdir/diag.sh start-elasticsearch
./msleep 5000
. $srcdir/diag.sh stop-elasticsearch
. $srcdir/diag.sh start-elasticsearch
shutdown_when_empty
wait_shutdown
./msleep 1000 # ES might need some time to maintain index...
. $srcdir/diag.sh es-getdata 10000 19200
. $srcdir/diag.sh stop-elasticsearch

seq_check  0 9999 -d
. $srcdir/diag.sh cleanup-elasticsearch
exit_test
