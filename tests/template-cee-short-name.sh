#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Regression test for string templates whose CEE/local-variable property name
# is the short indicator alone ("%!%", "%.%") or uses the short "!name" form.
# The template parser used to strip a two-byte "$!"/"$." prefix
# unconditionally, so a one-byte name read past the end of the parsed
# property name and underflowed the field-name length, and "%!str%" got the
# field name "tr". Sanitizer CI lanes catch the over-read; the functional
# oracle is the jsonf output, whose key is the stripped field name: it must be
# "str" for "%!str:::jsonf%", and the bare-indicator properties must render
# without error.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string"
	 string="[%!str:::jsonf%][%.lvar:::jsonf%][%!%][%.%]\n")
if $msg contains "msgnum" then {
	set $!str = "a";
	set $.lvar = "b";
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
export EXPECTED='["str":"a"]["lvar":"b"][{ "str": "a" }][{ "lvar": "b" }]'
cmp_exact
exit_test
