#!/bin/bash
# check disabling a config construct via config.enable. Most
# importantly ensure that it does not emit any error messages
# for object parameters.
# add 2019-05-09 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" file="/tmp/notyet.txt" tag="testing-tag" config.enabled="off")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'
startup
shutdown_when_empty
wait_shutdown
content_check 'imfile: no files configured'
check_not_present 'parameter .* not known'
exit_test
