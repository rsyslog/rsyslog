#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Unix timestamps are signed seconds since the UTC epoch, even when a valid
# local-year-1970 RFC5424 timestamp crosses into the preceding UTC day.
# Exact timestamp echoes prove parsing retained the input; explicit epoch
# values cover negative crossing, zero and positive/negative offset controls.
# Both string/list templates must match after queue-empty normal shutdown.
# This test does not broaden pre-1970 local-year or post-2038 platform policy.
. ${srcdir:=.}/diag.sh init
generate_conf
if [ "${EPOCH_YAML:-0}" = 1 ]; then
    add_conf 'include(file="'"$TESTCONF_NM"'.yaml")'
    add_yaml_conf 'templates:
  - name: legacyfmt
    type: string
    string: "%timereported:::date-rfc3339%|%timereported:::date-unixtimestamp%\n"
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
          dateformat: unixtimestamp
      - constant:
          value: "\n"
rulesets:
  - name: epoch-main
    actions:
      - type: omfile
        template: legacyfmt
        file: "'"$RSYSLOG_OUT_LOG"'"
      - type: omfile
        template: listfmt
        file: "'"$RSYSLOG_OUT_LOG"'.list"'
else
    add_conf '
template(name="legacyfmt" type="string"
 string="%timereported:::date-rfc3339%|%timereported:::date-unixtimestamp%\n")
template(name="listfmt" type="list") {
 property(name="timereported" dateformat="rfc3339")
 constant(value="|")
 property(name="timereported" dateformat="unixtimestamp")
 constant(value="\n")
}
ruleset(name="epoch-main") {
 action(type="omfile" template="legacyfmt" file="'"$RSYSLOG_OUT_LOG"'")
 action(type="omfile" template="listfmt" file="'"$RSYSLOG_OUT_LOG"'.list")
}'
fi
add_conf '
if $syslogtag == "epoch" then call epoch-main
'
rsyslogd_config_check || error_exit 1
startup
for stamp in 1970-01-01T00:30:00+02:00 1970-01-01T00:59:59+01:00 \
             1970-01-01T00:00:00Z \
             1970-01-01T02:30:00+02:00 1970-01-01T00:30:00-02:00; do
    injectmsg_literal "<34>1 $stamp host.example epoch - ID47 - probe"
done
shutdown_when_empty
wait_shutdown
export EXPECTED='1970-01-01T00:30:00+02:00|-5400
1970-01-01T00:59:59+01:00|-1
1970-01-01T00:00:00Z|0
1970-01-01T02:30:00+02:00|1800
1970-01-01T00:30:00-02:00|9000'
cmp_exact
cmp_exact "$RSYSLOG_OUT_LOG.list"
exit_test
