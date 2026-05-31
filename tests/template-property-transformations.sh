#!/bin/bash
# This focused template/property test covers deterministic MsgGetProp
# transformations without network timing: field extraction, substring bounds,
# regex match/no-match handling, control-character modes, secure-path
# sanitizing, and CSV/JSON formatting. The exact omfile output is the oracle.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="field2=")
	property(name="$!fields" field.number="2" field.delimiter="44")
	constant(value="\nfield_missing=")
	property(name="$!fields" field.number="5" field.delimiter="44")
	constant(value="\nsubstr=")
	property(name="$!word" position.from="2" position.to="4")
	constant(value="\nrelend=")
	property(name="$!word" position.from="3" position.to="1" position.relativetoend="on")
	constant(value="\nfixed=")
	property(name="$!short" position.from="1" position.to="5" fixedwidth="on")
	constant(value="|\nregex_second=")
	property(name="$!regexsrc" regex.expression="([a-z]+)-([0-9]+)" regex.type="ERE" regex.match="1" regex.submatch="2")
	constant(value="\nregex_zero=")
	property(name="$!word" regex.expression="ZZZ" regex.type="ERE" regex.nomatchmode="ZERO")
	constant(value="\ncc_drop=")
	property(name="$!control" controlcharacters="drop")
	constant(value="\ncc_space=")
	property(name="$!control" controlcharacters="space")
	constant(value="\ncc_escape=")
	property(name="$!control" controlcharacters="escape")
	constant(value="\nsec_drop=")
	property(name="$!path" securepath="drop")
	constant(value="\nsec_replace=")
	property(name="$!path" securepath="replace")
	constant(value="\nsec_empty=")
	property(name="$!empty" securepath="drop")
	constant(value="\nsec_dot=")
	property(name="$!dot" securepath="drop")
	constant(value="\nsec_dotdot=")
	property(name="$!dotdot" securepath="drop")
	constant(value="\ncsv=")
	property(name="$!csvsrc" format="csv")
	constant(value="\njson=")
	property(name="$!jsonsrc" format="json")
	constant(value="\njsonf=")
	property(name="$!jsonsrc" outname="jsonsrc" format="jsonf")
	constant(value="\n")
}

local4.* {
	set $!fields = "one,two,,four";
	set $!word = "alphabet";
	set $!short = "xy";
	set $!regexsrc = "abc-123 def-456";
	set $!control = "a\nb\tc";
	set $!path = "a/b/c";
	set $!empty = "";
	set $!dot = ".";
	set $!dotdot = "..";
	set $!csvsrc = "a,\"b\"";
	set $!jsonsrc = "a \\ \"b\"";
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

startup
injectmsg_literal '<167>1 2003-03-01T01:00:00.000Z host app proc msgid - trigger'
shutdown_when_empty
wait_shutdown

export EXPECTED='field2=two
field_missing=**FIELD NOT FOUND**
substr=lph
relend=bet
fixed=xy   |
regex_second=456
regex_zero=0
cc_drop=abc
cc_space=a b c
cc_escape=a#010b#009c
sec_drop=abc
sec_replace=a_b_c
sec_empty=_
sec_dot=_
sec_dotdot=_.
csv="a,""b"""
json=a \\ \"b\"
jsonf="jsonsrc":"a \\ \"b\""'
cmp_exact
exit_test
