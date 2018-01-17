#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
export ES_PORT=19200
. $srcdir/diag.sh download-elasticsearch
. $srcdir/diag.sh stop-elasticsearch
. $srcdir/diag.sh prepare-elasticsearch
. $srcdir/diag.sh start-elasticsearch

. $srcdir/diag.sh init
. $srcdir/diag.sh es-init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="tpl" type="string"
	 string="{\"msgnum\":\"%msg:F,58:2%\"}")

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
:msg, contains, "msgnum:" action(type="omelasticsearch"
				 template="tpl"
				 serverport=`echo $ES_PORT`
				 searchIndex="rsyslog_testbench"
				 bulkmode="on"
				 errorFile="./rsyslog.errorfile")
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 10000
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown 
. $srcdir/diag.sh es-getdata 10000 $ES_PORT
if [ -f rsyslog.errorfile ]
then
    echo "error: error file exists!"
    cat rsyslog.errorfile
    . $srcdir/diag.sh error-exit 1
fi
. $srcdir/diag.sh seq-check  0 9999 19200
. $srcdir/diag.sh stop-elasticsearch
. $srcdir/diag.sh cleanup-elasticsearch
. $srcdir/diag.sh exit
