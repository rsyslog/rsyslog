#!/bin/bash
# This focused template/property test covers deterministic MsgGetProp
# transformations without network timing: field extraction, substring bounds,
# regex match/no-match handling, control-character modes, string modifiers,
# timestamp formatting, secure-path sanitizing, CSV/JSON formatting, and the
# equivalent legacy string-template parser. One configuration renders all
# cases from fixed messages. The sorted omfile output is the oracle:
# imdiag-injected messages may reach the action queue in a different order
# under parallel CI, but every rendered property line and duplicate count must
# match exactly.
# Note that spifno1stsp emits only a conditional separator space, not the
# original property value, so its expected output is intentionally just markers
# around an inserted or suppressed space.
. ${srcdir:=.}/diag.sh init

generate_conf
add_conf '
template(name="outfmt" type="list") {
	constant(value="field2=")
	property(name="$!fields" field.number="2" field.delimiter="44")
	constant(value="\nfield_empty=<")
	property(name="$!fields" field.number="3" field.delimiter="44")
	constant(value=">\nfield_final=<")
	property(name="$!fields" field.number="5" field.delimiter="44")
	constant(value=">")
	constant(value="\nfield_missing=")
	property(name="$!fields" field.number="6" field.delimiter="44")
	constant(value="\nsubstr=")
	property(name="$!word" position.from="2" position.to="4")
	constant(value="\nsubstr_neg_to=")
	property(name="$!word" position.from="2" position.to="-2")
	constant(value="\nsubstr_neg_underflow=")
	property(name="$!word" position.from="1" position.to="-99")
	constant(value="\nsubstr_beyond=")
	property(name="$!word" position.from="99" position.to="120")
	constant(value="\nsubstr_superset=")
	property(name="$!word" position.from="1" position.to="999")
	constant(value="\nrelend=")
	property(name="$!word" position.from="3" position.to="1" position.relativetoend="on")
	constant(value="\nrelend_underflow=")
	property(name="$!short" position.from="99" position.to="50" position.relativetoend="on")
	constant(value="\nfixed=")
	property(name="$!short" position.from="1" position.to="5" fixedwidth="on")
	constant(value="|\nregex_second=")
	property(name="$!regexsrc" regex.expression="([a-z]+)-([0-9]+)" regex.type="ERE" regex.match="1" regex.submatch="2")
	constant(value="\nregex_third=")
	property(name="$!regexsrc" regex.expression="([a-z]+)-([0-9]+)" regex.type="ERE" regex.match="2" regex.submatch="2")
	constant(value="\nregex_optional_blank=<")
	property(name="$!optional" regex.expression="([a-z]+)(-([0-9]+))?" regex.type="ERE" regex.submatch="3" regex.nomatchmode="BLANK")
	constant(value=">")
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
	constant(value="\ncompress_msg=")
	property(name="msg" compressspace="on")
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
	constant(value="\ncc_escape_octal=")
	property(name="$!control" controlcharacters="escape-octal")
	constant(value="\nsec_drop=")
	property(name="$!path" securepath="drop")
	constant(value="\nsec_replace=")
	property(name="$!path" securepath="replace")
	constant(value="\nsec_replace_msg=")
	property(name="msg" securepath="replace")
	constant(value="\nsec_empty=")
	property(name="$!empty" securepath="drop")
	constant(value="\nsec_dot=")
	property(name="$!dot" securepath="drop")
	constant(value="\nsec_dotdot=")
	property(name="$!dotdot" securepath="drop")
	constant(value="\ncsv=")
	property(name="$!csvsrc" format="csv")
	constant(value="\ncombo_msg=")
	property(name="msg" securepath="replace" compressspace="on" format="csv")
	constant(value="\njson=")
	property(name="$!jsonsrc" format="json")
	constant(value="\njsonf=")
	property(name="$!jsonsrc" outname="jsonsrc" format="jsonf")
	constant(value="\njsonr=")
	property(name="$!jsonrsrc" format="jsonr")
	constant(value="\njsonfr=")
	property(name="$!jsonrsrc" outname="jsonrsrc" format="jsonfr")
	constant(value="\njson_zero=<")
	property(name="$!zero" outname="zero" format="jsonf" datatype="number" omitIfZero="on")
	constant(value=">\njson_empty=<")
	property(name="$!empty" outname="empty" format="jsonf" onEmpty="skip")
	constant(value=">\njson_auto=<")
	property(name="$!auto" outname="auto" format="jsonf" datatype="auto")
	constant(value=">\njson_false=<")
	property(name="$!false" outname="false" format="jsonf" datatype="bool")
	constant(value=">")
	constant(value="\nmsg=")
	property(name="msg")
	constant(value="\nhostname=")
	property(name="hostname")
	constant(value="\nsyslogtag=")
	property(name="syslogtag")
	constant(value="\nrawmsg_after_pri=")
	property(name="rawmsg-after-pri")
	constant(value="\npri=")
	property(name="pri")
	constant(value="\npri_text=")
	property(name="pri-text")
	constant(value="\niut=")
	property(name="iut")
	constant(value="\nfacility=")
	property(name="syslogfacility")
	constant(value="\nfacility_text=")
	property(name="syslogfacility-text")
	constant(value="\nseverity=")
	property(name="syslogseverity")
	constant(value="\nseverity_text=")
	property(name="syslogseverity-text")
	constant(value="\nprogramname=")
	property(name="programname")
	constant(value="\nprotocol=")
	property(name="protocol-version")
	constant(value="\nstructured_data=")
	property(name="structured-data")
	constant(value="\napp_name=")
	property(name="app-name")
	constant(value="\nprocid=")
	property(name="procid")
	constant(value="\nmsgid=")
	property(name="msgid")
	constant(value="\nparsesuccess=")
	property(name="parsesuccess")
	constant(value="\nreported_utc=")
	property(name="timereported" dateformat="rfc3339" date.inUTC="on")
	constant(value="\nreported_local_mysql=")
	property(name="timereported" dateformat="mysql")
	constant(value="\nreported_local_pgsql=")
	property(name="timereported" dateformat="pgsql")
	constant(value="\nreported_local_rfc3164=")
	property(name="timereported" dateformat="rfc3164")
	constant(value="\nreported_local_rfc3164_buggy=")
	property(name="timereported" dateformat="rfc3164-buggyday")
	constant(value="\nreported_local_unix=")
	property(name="timereported" dateformat="unixtimestamp")
	constant(value="\nreported_local_subseconds=")
	property(name="timereported" dateformat="subseconds")
	constant(value="\nreported_local_misc=")
	property(name="timereported" dateformat="wdayname")
	constant(value="/")
	property(name="timereported" dateformat="wday")
	constant(value="/")
	property(name="timereported" dateformat="tzoffshour")
	constant(value=":")
	property(name="timereported" dateformat="tzoffsmin")
	constant(value="/")
	property(name="timereported" dateformat="tzoffsdirection")
	constant(value="/")
	property(name="timereported" dateformat="ordinal")
	constant(value="/")
	property(name="timereported" dateformat="week")
	constant(value="\nreported_utc_formats=")
	property(name="timereported" dateformat="mysql" date.inUTC="on")
	constant(value="/")
	property(name="timereported" dateformat="pgsql" date.inUTC="on")
	constant(value="/")
	property(name="timereported" dateformat="rfc3164" date.inUTC="on")
	constant(value="/")
	property(name="timereported" dateformat="unixtimestamp" date.inUTC="on")
	constant(value="/")
	property(name="timereported" dateformat="subseconds" date.inUTC="on")
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

template(name="legacyfmt" type="string"
	string="legacy_upper=%$!mixed:::uppercase%\nlegacy_fixed=%$!short:1:5:fixed-width%|\nlegacy_relend=%$!word:3:1:pos-end-relative%\nlegacy_swap=%$!word:4:2%\nlegacy_field=%$!fields:F,44:2%\nlegacy_escape=%$!control:::escape-cc%\nlegacy_sec_replace=%$!path:::secpath-replace%\nlegacy_jsonf=%$!jsonsrc:::jsonf%\nlegacy_jsonr=%$!jsonrsrc:::jsonr%\nlegacy_jsonfr=%$!jsonrsrc:::jsonfr%\nlegacy_date=%timereported:::date-wdayname%/%timereported:::date-week%\n")

template(name="shapefmt" type="list") {
	constant(value="shape_msg=")
	property(name="msg")
	constant(value="\nshape_hostname=")
	property(name="hostname")
	constant(value="\nshape_syslogtag=")
	property(name="syslogtag")
	constant(value="\nshape_programname=")
	property(name="programname")
	constant(value="\nshape_protocol=")
	property(name="protocol-version")
	constant(value="\nshape_structured_data=")
	property(name="structured-data")
	constant(value="\nshape_app_name=")
	property(name="app-name")
	constant(value="\nshape_procid=")
	property(name="procid")
	constant(value="\nshape_msgid=")
	property(name="msgid")
	constant(value="\nshape_rawmsg_after_pri=")
	property(name="rawmsg-after-pri")
	constant(value="\n")
}

local4.* {
	if ($rawmsg contains "shape") then {
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="shapefmt")
	} else {
		set $!fields = "one,two,,four,";
		set $!word = "alphabet";
		set $!short = "xy";
		set $!regexsrc = "abc-123 def-456 ghi-789";
		set $!optional = "abc";
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
		set $!zero = 0;
		set $!auto = 42;
		set $!false = 0;
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
		action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="legacyfmt")
	}
}
'

startup
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 host/name app proc msgid - trigger/path  a  b'
injectmsg_literal '<167>Aug 24 05:14:15 legacyhost legacyprog[42]: shape3164'
injectmsg_literal '<167>1 2003-08-24T05:14:15.000003-07:00 nilhost - - - - shape5424nil'
injectmsg_literal '<167>Aug 24 05:14:15 oddhost shape3164notag'
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 115
shutdown_when_empty
wait_shutdown

export EXPECTED='field2=two
field_empty=<>
field_final=<>
field_missing=**FIELD NOT FOUND**
substr=lph
substr_neg_to=lphab
substr_neg_underflow=a
substr_beyond=
substr_superset=alphabet
relend=bet
relend_underflow=x
fixed=xy   |
regex_second=456
regex_third=789
regex_optional_blank=<>
regex_default=**NO MATCH**
regex_blank=<>
regex_field=alphabet
regex_zero=0
upper=MIXED
lower=mixed
compress=a b c
compress_msg=trigger/path a b
droplastlf=tail
spif_nonspace=< >
spif_space=<>
cc_drop=abc
cc_space=a b c
cc_escape=a#010b#009c
cc_escape_octal=a#012b#011c
sec_drop=abc
sec_replace=a_b_c
sec_replace_msg=trigger_path  a  b
sec_empty=_
sec_dot=_
sec_dotdot=_.
csv="a,""b"""
combo_msg="trigger_path a b"
json=a \\ \"b\"
jsonf="jsonsrc":"a \\ \"b\""
jsonr=a \n b
jsonfr="jsonrsrc":"a \n b"
json_zero=<>
json_empty=<>
json_auto=<"auto":42>
json_false=<"false":false>
msg=trigger/path  a  b
hostname=host/name
syslogtag=app[proc]
rawmsg_after_pri=1 2003-08-24T05:14:15.000003-07:00 host/name app proc msgid - trigger/path  a  b
pri=167
pri_text=local4.debug
iut=1
facility=20
facility_text=local4
severity=7
severity_text=debug
programname=app
protocol=1
structured_data=-
app_name=app
procid=proc
msgid=msgid
parsesuccess=FAIL
reported_utc=2003-08-24T12:14:15.000003+00:00
reported_local_mysql=20030824051415
reported_local_pgsql=2003-08-24 05:14:15
reported_local_rfc3164=Aug 24 05:14:15
reported_local_rfc3164_buggy=Aug 24 05:14:15
reported_local_unix=1061727255
reported_local_subseconds=000003
reported_local_misc=Sun/0/07:00/-/236/35
reported_utc_formats=20030824121415/2003-08-24 12:14:15/Aug 24 12:14:15/1061727255/000003
reported_parts=2003-08-24T12:14:15
legacy_upper=MIXED
legacy_fixed=xy   |
legacy_relend=bet
legacy_swap=lph
legacy_field=two
legacy_escape=a#010b#009c
legacy_sec_replace=a_b_c
legacy_jsonf="jsonsrc":"a \\ \"b\""
legacy_jsonr=a \n b
legacy_jsonfr="jsonrsrc":"a \n b"
legacy_date=Sun/35
shape_msg= shape3164
shape_hostname=legacyhost
shape_syslogtag=legacyprog[42]:
shape_programname=legacyprog
shape_protocol=0
shape_structured_data=-
shape_app_name=legacyprog
shape_procid=42
shape_msgid=-
shape_rawmsg_after_pri=Aug 24 05:14:15 legacyhost legacyprog[42]: shape3164
shape_msg=shape5424nil
shape_hostname=nilhost
shape_syslogtag=-
shape_programname=-
shape_protocol=1
shape_structured_data=-
shape_app_name=-
shape_procid=-
shape_msgid=-
shape_rawmsg_after_pri=1 2003-08-24T05:14:15.000003-07:00 nilhost - - - - shape5424nil
shape_msg=
shape_hostname=oddhost
shape_syslogtag=shape3164notag
shape_programname=shape3164notag
shape_protocol=0
shape_structured_data=-
shape_app_name=shape3164notag
shape_procid=-
shape_msgid=-
shape_rawmsg_after_pri=Aug 24 05:14:15 oddhost shape3164notag'
expected_sorted="${RSYSLOG_DYNNAME}.expected.sorted"
actual_sorted="${RSYSLOG_DYNNAME}.actual.sorted"
printf '%s\n' "$EXPECTED" | LC_ALL=C $RS_SORTCMD > "$expected_sorted"
LC_ALL=C $RS_SORTCMD "$RSYSLOG_OUT_LOG" > "$actual_sorted"
cmp_exact_file "$expected_sorted" "$actual_sorted"
exit_test
