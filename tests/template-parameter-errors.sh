#!/bin/bash
# Exercise malformed legacy property-replacer parameters one configuration at
# a time. Each case must make rsyslogd -N1 fail under abortOnUncleanConfig and
# emit its specific parser diagnostic; separate invocations prove that an
# earlier rejected template cannot hide later cases.
# The long decimal literals deliberately overflow parser integers; they are
# configuration data, not fixed network ports.
. ${srcdir:=.}/diag.sh init

run_invalid_parameter() {
	local name="$1"
	local parameter="$2"
	local expected="$3"
	local cfg="${RSYSLOG_DYNNAME}.${name}.conf"
	local log="${RSYSLOG_DYNNAME}.${name}.log"

	cat >"$cfg" <<CONF_EOF
global(abortOnUncleanConfig="on")
template(name="$name" type="string" string="$parameter")
CONF_EOF

	if ../tools/rsyslogd -C -N1 -f"$cfg" -M"$RSYSLOG_MODDIR" >"$log" 2>&1; then
		error_exit 1 "config check unexpectedly succeeded for $name"
	fi
	content_check "$expected" "$log"
}

while IFS='|' read -r name parameter expected; do
	run_invalid_parameter "$name" "$parameter" "$expected"
done <<'CASES'
invalid_option|%msg:::bogus-option%|template error: invalid field option 'bogus-option' specified - ignored
conflict_csv|%msg:::json,csv%|one option out of (json, jsonf, jsonr, jsonfr, csv) - csv ignored
conflict_json|%msg:::csv,json%|one option out of (json, jsonf, jsonr, jsonfr, csv) - json ignored
conflict_jsonf|%msg:::csv,jsonf%|one option out of (json, jsonf, jsonr, jsonfr, csv) - jsonf ignored
conflict_jsonr|%msg:::csv,jsonr%|one option out of (json, jsonf, jsonr, jsonfr, csv) - jsonr ignored
conflict_jsonfr|%msg:::csv,jsonfr%|one option out of (json, jsonf, jsonr, jsonfr, csv) - jsonfr ignored
delimiter_nondigit|%msg:F,x:2%|invalid character in frompos after "F,"
delimiter_overflow|%msg:F,999999999999999999999999:2%|delimiter value in template is too large
delimiter_nonascii|%msg:F,256:2%|non-USASCII delimiter character value 256
delimiter_tail|%msg:F,44x:2%|invalid character 'x' in frompos after "F,"
invalid_after_f|%msg:Fx:2%|invalid character in frompos after "F"
frompos_overflow|%msg:999999999999999999999999:2%|frompos value in template is too large
field_number_overflow|%msg:F,44:999999999999999999999999%|field number in template is too large
field_topos_overflow|%msg:F,44:2,999999999999999999999999%|topos value in template is too large
topos_overflow|%msg:1:999999999999999999999999%|topos value in template is too large
CASES

exit_test
