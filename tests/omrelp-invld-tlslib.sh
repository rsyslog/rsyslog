#!/bin/bash
# see that we can an error message if wrong tls lib is selected
# addd 2019-02-09 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
require_relpEngineSetTLSLibByName
generate_conf
add_conf '
module(load="../plugins/omrelp/.libs/omrelp" tls.tlslib="invalid-tlslib-name")

action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
content_check --regex "omrelp.*invalid-tlslib-name.*not accepted"
exit_test
