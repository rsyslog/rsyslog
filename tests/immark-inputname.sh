#!/bin/bash
# add 2020-12-02 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
module(load="../plugins/immark/.libs/immark" interval="1" use.syslogcall="off")
if $inputname == "immark" then
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'")
'

startup
printf 'sleeping a bit so we get mark messages...\n'
sleep 3 # this should be good even on slow machines - we need just one
shutdown_when_empty
wait_shutdown
content_check "rsyslogd: -- MARK --"

exit_test
