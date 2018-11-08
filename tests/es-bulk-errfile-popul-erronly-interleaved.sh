#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
export ES_DOWNLOAD=elasticsearch-6.0.0.tar.gz
download_elasticsearch
prepare_elasticsearch
start_elasticsearch

init_elasticsearch
echo '{ "name" : "foo" }
{"name": bar"}
{"name": "baz"}
{"name": foz"}' > inESData.inputfile
generate_conf
add_conf '
# Note: we must mess up with the template, because we can not
# instruct ES to put further constraints on the data type and
# values. So we require integer and make sure it is none.
template(name="tpl" type="list") {
	 constant(value="{\"")        
        property(name="$!key") constant(value="\":") property(name="$!obj")
      constant(value="}")   
}

module(load="../plugins/omelasticsearch/.libs/omelasticsearch")
module(load="../plugins/imfile/.libs/imfile")
ruleset(name="foo") {
  set $!key = "my_obj";
  set $!obj = $msg;
  action(type="omelasticsearch"
	 template="tpl"
	 searchIndex="rsyslog_testbench"
	 searchType="test-type"
	 bulkmode="on"
	 serverport="19200"
	 errorFile="./'${RSYSLOG_DYNNAME}'.errorfile"
	erroronly="on"		 
	interleaved="on")
}

input(type="imfile" File="./inESData.inputfile"
      Tag="foo"
      StateFile="stat-file1"
      Severity="info"
      ruleset="foo")
'
startup
shutdown_when_empty
wait_shutdown
rm -f inESData.inputfile

python $srcdir/elasticsearch-error-format-check.py errorinterleaved

if [ $? -ne 0 ]
then
    echo "error: Format for error file different! " $?
    exit 1
fi
cleanup_elasticsearch
exit_test
