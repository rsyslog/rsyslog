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
set $!lower_nr  = '$LOWER_VAL';
set $!higher_nr = '$HIGHER_VAL';

if $!lower_nr <= $!higher_nr
	then {  set $!result = "<= RIGHT"; }
	else {  set $!result = "<= WRONG"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $!lower_nr < $!higher_nr
	then {  set $!result = "<  RIGHT"; }
	else {  set $!result = "<  WRONG"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $!higher_nr >= $!lower_nr
	then {  set $!result = ">= RIGHT"; }
	else {  set $!result = ">= WRONG"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $!higher_nr > $!lower_nr
	then {  set $!result = ">  RIGHT"; }
	else {  set $!result = ">  WRONG"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $!higher_nr != $!lower_nr
	then {  set $!result = "!= RIGHT"; }
	else {  set $!result = "!= WRONG"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")

if $!higher_nr == $!lower_nr
	then {  set $!result = "== WRONG"; }
	else {  set $!result = "== RIGHT"; }
action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
'
startup
shutdown_when_empty
wait_shutdown 
content_check '<= RIGHT'
content_check '>= RIGHT'
content_check '!= RIGHT'
content_check '== RIGHT'
content_check '<  RIGHT'
content_check '>  RIGHT'
exit_test
