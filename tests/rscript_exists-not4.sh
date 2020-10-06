#!/bin/bash
# add 2020-10-02 by Rainer Gerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%!result%\n")
set $.somevar = "test"; # this makes matters a bit more complicated
if $msg contains "msgnum" then {
	if not exists($.p1!p2!val) then
		set $!result = "off";
	else
		set $!result = "on";
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
	}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
export EXPECTED='off'
cmp_exact
exit_test
