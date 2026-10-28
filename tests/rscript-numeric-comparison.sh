#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Generic comparisons must order full-width signed numbers without narrowing
# their difference. Both operands are variables to bypass constant/PRI folding.
# Explicit expected truth vectors cover all six operators, signed boundaries,
# numeric-string conversion and lexical fallback. One injected message executes
# every case; exact omfile output after synchronized shutdown is the oracle.
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf 'template(name="outfmt" type="string" string="%$.case%:%$.lt%:%$.le%:%$.gt%:%$.ge%:%$.eq%:%$.ne%\n")'
add_conf 'if $msg contains "msgnum:" then {'
: > "$RSYSLOG_DYNNAME.expected"
while IFS='|' read -r label left right expected; do
    add_conf "set \$.case = \"$label\";
set \$.left = $left;
set \$.right = $right;
if (\$.left < \$.right) then set \$.lt = 1; else set \$.lt = 0;
if (\$.left <= \$.right) then set \$.le = 1; else set \$.le = 0;
if (\$.left > \$.right) then set \$.gt = 1; else set \$.gt = 0;
if (\$.left >= \$.right) then set \$.ge = 1; else set \$.ge = 0;
if (\$.left == \$.right) then set \$.eq = 1; else set \$.eq = 0;
if (\$.left != \$.right) then set \$.ne = 1; else set \$.ne = 0;
action(type=\"omfile\" file=\"$RSYSLOG_OUT_LOG\" template=\"outfmt\")"
    printf '%s:%s\n' "$label" "$expected" >> "$RSYSLOG_DYNNAME.expected"
done <<'CASES'
ordinary-less|1|2|1:1:0:0:0:1
ordinary-greater|2|1|0:0:1:1:0:1
ordinary-equal|2|2|0:1:0:1:1:0
wide-less|0|4294967296|1:1:0:0:0:1
wide-greater|4294967296|0|0:0:1:1:0:1
wide-equal|4294967296|4294967296|0:1:0:1:1:0
negative-less|-4294967296|0|1:1:0:0:0:1
negative-greater|0|-4294967296|0:0:1:1:0:1
negative-equal|-4294967296|-4294967296|0:1:0:1:1:0
max-less|0|9223372036854775807|1:1:0:0:0:1
max-greater|9223372036854775807|0|0:0:1:1:0:1
max-equal|9223372036854775807|9223372036854775807|0:1:0:1:1:0
min-less|(-9223372036854775807 - 1)|0|1:1:0:0:0:1
min-greater|0|(-9223372036854775807 - 1)|0:0:1:1:0:1
min-equal|(-9223372036854775807 - 1)|(-9223372036854775807 - 1)|0:1:0:1:1:0
extremes-less|(-9223372036854775807 - 1)|9223372036854775807|1:1:0:0:0:1
extremes-greater|9223372036854775807|(-9223372036854775807 - 1)|0:0:1:1:0:1
numeric-string-less|"2"|"10"|1:1:0:0:0:1
mixed-less|"2"|10|1:1:0:0:0:1
mixed-wide|0|"4294967296"|1:1:0:0:0:1
lexical-less|"alpha"|"beta"|1:1:0:0:0:1
lexical-greater|"beta"|"alpha"|0:0:1:1:0:1
lexical-equal|"alpha"|"alpha"|0:1:0:1:1:0
mixed-fallback|2|"a"|1:1:0:0:0:1
CASES
add_conf '}'
startup
injectmsg 0 1
shutdown_when_empty
wait_shutdown
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"
exit_test
