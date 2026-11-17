#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# fixedWidth pads a short source to position.to before selecting the range.
# Marker-delimited exact output proves spaces for empty/at-end/beyond-end
# ranges while preserving partial padding, truncation and non-fixed behavior.
# One literal message, queue-empty shutdown and proper termination synchronize
# the complete string/list output oracle; no timing threshold is used.
. ${srcdir:=.}/diag.sh init
cases='empty|$!empty|1|5|on
beyond|$!one|3|5|on
at_end|$!abc|3|5|on
after_end|$!abc|4|6|on
partial|$!short|1|5|on
exact|$!exact|1|5|on
long|$!long|1|5|on
empty_plain|$!empty|1|5|off
beyond_plain|$!one|3|5|off
short_plain|$!short|1|5|off'
legacy=''
while IFS='|' read -r label prop from to width; do
    if [ "$width" = on ]; then
        legacy="${legacy}${label}=<%${prop}:${from}:${to}:fixed-width%>\n"
    else
        legacy="${legacy}${label}=<%${prop}:${from}:${to}%>\n"
    fi
done <<< "$cases"
generate_conf
if [ "${FIXEDWIDTH_YAML:-0}" = 1 ]; then
    add_conf 'include(file="'"$TESTCONF_NM"'.yaml")'
    add_yaml_conf 'templates:'
    add_yaml_conf "  - name: legacyfmt
    type: string
    string: \"$legacy\"
  - name: listfmt
    type: list
    elements:"
    while IFS='|' read -r label prop from to width; do
        add_yaml_conf "      - constant:
          value: \"${label}=<\"
      - property:
          name: \"$prop\"
          position.from: \"$from\"
          position.to: \"$to\"
          fixedWidth: \"$width\"
      - constant:
          value: \">\\n\""
    done <<< "$cases"
    add_yaml_conf 'rulesets:
  - name: fixed-main
    actions:
      - type: omfile
        template: legacyfmt
        file: "'"$RSYSLOG_OUT_LOG"'"
      - type: omfile
        template: listfmt
        file: "'"$RSYSLOG_OUT_LOG"'.list"'
else
    add_conf "template(name=\"legacyfmt\" type=\"string\" string=\"$legacy\")"
    add_conf 'template(name="listfmt" type="list") {'
    while IFS='|' read -r label prop from to width; do
        add_conf "constant(value=\"${label}=<\")
property(name=\"$prop\" position.from=\"$from\" position.to=\"$to\" fixedWidth=\"$width\")
constant(value=\">\\n\")"
    done <<< "$cases"
    add_conf '}
ruleset(name="fixed-main") {
 action(type="omfile" template="legacyfmt" file="'"$RSYSLOG_OUT_LOG"'")
 action(type="omfile" template="listfmt" file="'"$RSYSLOG_OUT_LOG"'.list")
}'
fi
add_conf '
if $syslogtag == "fixedwidth" then {
 set $!empty = "";
 set $!one = "A";
 set $!abc = "abc";
 set $!short = "ab";
 set $!exact = "abcde";
 set $!long = "abcdef";
 call fixed-main
}
'
rsyslogd_config_check || error_exit 1
startup
injectmsg_literal "<34>1 2026-01-02T12:00:00Z host.example fixedwidth - ID47 - probe"
shutdown_when_empty
wait_shutdown
export EXPECTED='empty=<     >
beyond=<   >
at_end=<c  >
after_end=<   >
partial=<ab   >
exact=<abcde>
long=<abcde>
empty_plain=<>
beyond_plain=<>
short_plain=<ab>'
cmp_exact
cmp_exact "$RSYSLOG_OUT_LOG.list"
exit_test
