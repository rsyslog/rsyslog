#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Calendar ordinals must follow the valid RFC5424 timestamp's local date,
# independently of its positive/negative UTC offset and the host timezone.
# Explicit local/UTC expectations cover near-midnight offsets, leap day and
# year end; echoed timestamps prove parsing retained the supplied offset.
# Both string and list templates must match after synchronized normal shutdown.
. ${srcdir:=.}/diag.sh init

if [ "${ORDINAL_YAML:-0}" = 1 ]; then
    generate_conf
    add_conf 'include(file="'"$TESTCONF_NM"'.yaml")
if $syslogtag == "ordinal" then call ordinal-main'
    add_yaml_conf '
templates:
  - name: legacyfmt
    type: string
    string: "%timereported:::date-rfc3339%|%timereported:::date-ordinal%|%timereported:::date-ordinal,date-utc%\n"
  - name: listfmt
    type: list
    elements:
      - property:
          name: timereported
          dateformat: rfc3339
      - constant:
          value: "|"
      - property:
          name: timereported
          dateformat: ordinal
      - constant:
          value: "|"
      - property:
          name: timereported
          dateformat: ordinal
          date.inUTC: "on"
      - constant:
          value: "\n"
rulesets:
  - name: ordinal-main
    actions:
      - type: omfile
        template: legacyfmt
        file: "'"$RSYSLOG_OUT_LOG"'"
      - type: omfile
        template: listfmt
        file: "'"$RSYSLOG_OUT_LOG"'.list"
'
else
    generate_conf
    add_conf '
template(name="legacyfmt" type="string"
 string="%timereported:::date-rfc3339%|%timereported:::date-ordinal%|%timereported:::date-ordinal,date-utc%\n")
template(name="listfmt" type="list") {
 property(name="timereported" dateformat="rfc3339")
 constant(value="|")
 property(name="timereported" dateformat="ordinal")
 constant(value="|")
 property(name="timereported" dateformat="ordinal" date.inUTC="on")
 constant(value="\n")
}
if $syslogtag == "ordinal" then {
 action(type="omfile" template="legacyfmt" file="'"$RSYSLOG_OUT_LOG"'")
 action(type="omfile" template="listfmt" file="'"$RSYSLOG_OUT_LOG"'.list")
}
'
fi
rsyslogd_config_check || error_exit 1
startup
for stamp in 2026-01-02T00:30:00+02:00 2026-01-02T00:30:00+05:30 \
             2026-01-02T12:00:00+02:00 2026-01-02T00:30:00Z \
             2026-01-02T00:30:00-02:00 2024-03-01T00:30:00+02:00 \
             2026-12-31T00:30:00+02:00; do
    injectmsg_literal "<34>1 $stamp host.example ordinal - ID47 - probe"
done
shutdown_when_empty
wait_shutdown
export EXPECTED='2026-01-02T00:30:00+02:00|002|001
2026-01-02T00:30:00+05:30|002|001
2026-01-02T12:00:00+02:00|002|002
2026-01-02T00:30:00Z|002|002
2026-01-02T00:30:00-02:00|002|002
2024-03-01T00:30:00+02:00|061|060
2026-12-31T00:30:00+02:00|365|364'
cmp_exact
cmp_exact "$RSYSLOG_OUT_LOG.list"
exit_test
