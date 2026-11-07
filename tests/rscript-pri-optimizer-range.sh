#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Numeric PRI constants must be range-checked before conversion to int.
# Invalid constants must fail -N1 and report the complete original value:
# process status and config-check diagnostics are the intended negative oracle.
# Runtime controls compare optimized valid constants with a generic numeric
# variable for all wire PRI values and all six comparison operators. Wide
# variable operands remain legal generic comparisons. Synchronized shutdown
# precedes exact output checking; no timing threshold is used.
. ${srcdir:=.}/diag.sh init

for property in syslogseverity syslogfacility; do
    max=7
    [ "$property" = syslogfacility ] && max=24
    for value in -1 "$((max + 1))" 4294967296 "$((4294967296 + max))" \
                 -4294967296 "$((-4294967296 + max))" \
                 9223372036854775807 -9223372036854775807; do
        for operator in == != '<' '<=' '>' '>='; do
            generate_conf
            add_conf "if \$$property $operator $value then action(type=\"omfile\" file=\"$RSYSLOG_OUT_LOG\")"
            if rsyslogd_config_check > "$RSYSLOG_DYNNAME.preflight" 2>&1; then
                echo "accepted invalid $property $operator $value"
                error_exit 1
            fi
            if ! grep -F "invalid $property $value," "$RSYSLOG_DYNNAME.preflight"; then
                cat "$RSYSLOG_DYNNAME.preflight"
                error_exit 1 "missing full-value diagnostic for $property $operator $value"
            fi
        done
    done
done

generate_conf
add_conf '
template(name="outfmt" type="string" string="%syslogfacility%:%syslogseverity%:%$.errors%\n")
if $msg contains "PRI-CONTROL" then {
set $.errors = 0;
set $.severity = $syslogseverity;
set $.facility = $syslogfacility;
'
for property in severity facility; do
    max=7
    [ "$property" = facility ] && max=24
    for value in 0 "$max"; do
        for operator in == != '<' '<=' '>' '>='; do
            add_conf "if (\$syslog$property $operator $value) != (\$.$property $operator $value) then set \$.errors = \$.errors + 1;"
        done
    done
    for value in 4294967296 -4294967296; do
        add_conf "set \$.wide = $value;"
        for operator in == != '<' '<=' '>' '>='; do
            add_conf "if (\$syslog$property $operator \$.wide) != (\$.$property $operator \$.wide) then set \$.errors = \$.errors + 1;"
        done
    done
done
add_conf 'action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")
}'
rsyslogd_config_check || error_exit 1 "valid controls rejected"
startup
: > "$RSYSLOG_DYNNAME.messages"
: > "$RSYSLOG_DYNNAME.expected"
for ((pri=0; pri<192; pri++)); do
    printf '<%s>Oct 28 12:00:00 localhost test: PRI-CONTROL\n' "$pri" >> "$RSYSLOG_DYNNAME.messages"
    printf '%s:%s:0\n' "$((pri / 8))" "$((pri % 8))" >> "$RSYSLOG_DYNNAME.expected"
done
injectmsg_file "$RSYSLOG_DYNNAME.messages"
shutdown_when_empty
wait_shutdown
sort "$RSYSLOG_DYNNAME.expected" > "$RSYSLOG_DYNNAME.sorted"
sort "$RSYSLOG_OUT_LOG" > "$RSYSLOG_DYNNAME.actual"
cmp_exact_file "$RSYSLOG_DYNNAME.sorted" "$RSYSLOG_DYNNAME.actual"
exit_test
