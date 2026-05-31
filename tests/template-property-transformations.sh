#!/bin/bash
# This focused template/property test covers deterministic MsgGetProp
# transformations without network timing: field extraction, substring bounds,
# regex match/no-match handling, control-character modes, string modifiers,
# timestamp formatting, secure-path sanitizing, and CSV/JSON formatting. The
# exact omfile output is the oracle.
# Note that spifno1stsp emits only a conditional separator space, not the
# original property value, so its expected output is intentionally just markers
# around an inserted or suppressed space.
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
	constant(value="\nsubstr_neg_to=")
	property(name="$!word" position.from="2" position.to="-2")
	constant(value="\nsubstr_beyond=")
	property(name="$!word" position.from="99" position.to="120")
	constant(value="\nsubstr_superset=")
	property(name="$!word" position.from="1" position.to="999")
	constant(value="\nrelend=")
	property(name="$!word" position.from="3" position.to="1" position.relativetoend="on")
	constant(value="\nfixed=")
	property(name="$!short" position.from="1" position.to="5" fixedwidth="on")
	constant(value="|\nregex_second=")
	property(name="$!regexsrc" regex.expression="([a-z]+)-([0-9]+)" regex.type="ERE" regex.match="1" regex.submatch="2")
	constant(value="\nregex_default=")
	property(name="$!word" regex.expression="ZZZ" regex.type="ERE" regex.nomatchmode="DFLT")
	constant(value="\nregex_blank=<")
	property(name="$!word" regex.expression="ZZZ" regex.type="ERE" regex.nomatchmode="BLANK")
	constant(value=">")
	constant(value="\nregex_field=")
	property(name="$!word" regex.expression="ZZZ" regex.type="ERE" regex.nomatchmode="FIELD")
	constant(value="\nregex_zero=")
	property(name="$!word" regex.expression="ZZZ" regex.type="ERE" regex.nomatchmode="ZERO")
	constant(value="\nupper=")
	property(name="$!mixed" caseconversion="upper")
	constant(value="\nlower=")
	property(name="$!mixed" caseconversion="lower")
	constant(value="\ncompress=")
	property(name="$!spaces" compressspace="on")
	constant(value="\ndroplastlf=")
	property(name="$!line" droplastlf="on")
	constant(value="\nspif_nonspace=<")
	property(name="$!word" spifno1stsp="on")
	constant(value=">")
	constant(value="\nspif_space=<")
	property(name="$!leading" spifno1stsp="on")
	constant(value=">")
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
	constant(value="\njsonr=")
	property(name="$!jsonrsrc" format="jsonr")
	constant(value="\njsonfr=")
	property(name="$!jsonrsrc" outname="jsonrsrc" format="jsonfr")
	constant(value="\nreported_utc=")
	property(name="timereported" dateformat="rfc3339" date.inUTC="on")
	constant(value="\nreported_parts=")
	property(name="timereported" dateformat="year" date.inUTC="on")
	constant(value="-")
	property(name="timereported" dateformat="month" date.inUTC="on")
	constant(value="-")
	property(name="timereported" dateformat="day" date.inUTC="on")
	constant(value="T")
	property(name="timereported" dateformat="hour" date.inUTC="on")
	constant(value=":")
	property(name="timereported" dateformat="minute" date.inUTC="on")
	constant(value=":")
	property(name="timereported" dateformat="second" date.inUTC="on")
	constant(value="\n")
}

local4.* {
	set $!fields = "one,two,,four";
	set $!word = "alphabet";
	set $!short = "xy";
	set $!regexsrc = "abc-123 def-456";
	set $!mixed = "MiXeD";
	set $!spaces = "a   b  c";
	set $!line = "tail\n";
	set $!leading = " lead";
	set $!control = "a\nb\tc";
	set $!path = "a/b/c";
	set $!empty = "";
	set $!dot = ".";
	set $!dotdot = "..";
	set $!csvsrc = "a,\"b\"";
	set $!jsonsrc = "a \\ \"b\"";
	set $!jsonrsrc = "a \\n b";
	action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}
'

startup
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 host app proc msgid - trigger'
shutdown_when_empty
wait_shutdown

export EXPECTED='field2=two
field_missing=**FIELD NOT FOUND**
substr=lph
substr_neg_to=lphab
substr_beyond=
substr_superset=alphabet
relend=bet
fixed=xy   |
regex_second=456
regex_default=**NO MATCH**
regex_blank=<>
regex_field=alphabet
regex_zero=0
upper=MIXED
lower=mixed
compress=a b c
droplastlf=tail
spif_nonspace=< >
spif_space=<>
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
jsonf="jsonsrc":"a \\ \"b\""
jsonr=a \n b
jsonfr="jsonrsrc":"a \n b"
reported_utc=2003-08-24T12:14:15.000003+00:00
reported_parts=2003-08-24T12:14:15'
cmp_exact
exit_test
