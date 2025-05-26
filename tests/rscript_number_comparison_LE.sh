#!/bin/bash
# added by rgerhards
# This file is part of the rsyslog project, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="list") {
	property(name="$!result")
	constant(value="\n")
}
set $!lower_nr  = 1111;
set $!higher_nr = 2222;

if $!lower_nr <= $!higher_nr
  then {  set $!result = "RIGHT"; }
  else {  set $!result = "WRONG"; }

action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown 
content_check 'RIGHT'
exit_test
