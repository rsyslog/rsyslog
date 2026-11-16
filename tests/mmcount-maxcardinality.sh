#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# mmcount must bound the number of distinct values it tracks per action.
# Values are taken from messages, so without a bound a sender can grow the
# counter table without limit. With maxCardinality=2, one injected message
# calls the counting ruleset with the values a,b,c,a,c,b in that fixed order:
# a and b get counters, c arrives when the table is full and must stay
# uncounted (no $!mmcount, rendered as "c:"), while the already tracked
# values a and b keep counting. $!mmcount is unset before every call because
# all calls share one message and would otherwise show the previous total. The exact omfile output after synchronized
# shutdown is the oracle; a second file must contain the one-time
# limit-reached warning exactly once.
. ${srcdir:=.}/diag.sh init
require_plugin mmcount ../contrib/mmcount
export NUMMESSAGES=6
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../contrib/mmcount/.libs/mmcount")
template(name="outfmt" type="string" string="%$!key%:%$!mmcount%\n")
template(name="warnfmt" type="string" string="%msg%\n")
if $syslogtag startswith "rsyslogd" and $msg contains "maxCardinality" then {
    action(type="omfile" file="'$RSYSLOG2_OUT_LOG'" template="warnfmt")
    stop
}
'
if [ "${MMCOUNT_TEST_YAML:-0}" = 1 ]; then
    require_yaml_support
    cat > "${RSYSLOG_DYNNAME}.yaml" <<YAMLEOF
rulesets:
  - name: countValue
    actions:
      - type: mmcount
        appname: counter
        key: "!key"
        maxcardinality: 2
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        template: outfmt
YAMLEOF
    add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")'
else
    add_conf '
ruleset(name="countValue") {
    action(type="mmcount" appname="counter" key="!key" maxCardinality="2")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'
fi
add_conf '
if $app-name == "counter" then {
    set $!key = "a";
    unset $!mmcount;
    call countValue
    set $!key = "b";
    unset $!mmcount;
    call countValue
    set $!key = "c";
    unset $!mmcount;
    call countValue
    set $!key = "a";
    unset $!mmcount;
    call countValue
    set $!key = "c";
    unset $!mmcount;
    call countValue
    set $!key = "b";
    unset $!mmcount;
    call countValue
}'
startup
injectmsg_literal '<13>1 2026-11-16T00:00:00Z localhost counter - - - trigger'
shutdown_when_empty
wait_shutdown
printf 'a:1\nb:1\nc:\na:2\nc:\nb:2\n' > "$RSYSLOG_OUT_LOG.expected"
cmp_exact_file "$RSYSLOG_OUT_LOG.expected" "$RSYSLOG_OUT_LOG"
content_count_check "reached maxCardinality 2" 1 "$RSYSLOG2_OUT_LOG"
exit_test
