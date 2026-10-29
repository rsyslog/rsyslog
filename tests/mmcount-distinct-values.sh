#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# mmcount must maintain distinct counters for distinct property values even
# when their hashes collide. "ab" and "bA" collide in hash_from_string.
# One injected message calls the same action four times, eliminating message
# ordering ambiguity. The exact omfile output after synchronized shutdown must
# show independent totals 1,1,2,2 rather than a shared 1,2,3,4 counter.
. ${srcdir:=.}/diag.sh init
require_plugin mmcount ../contrib/mmcount
export NUMMESSAGES=4
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
generate_conf
add_conf '
module(load="../contrib/mmcount/.libs/mmcount")
template(name="outfmt" type="string" string="%$!key%:%$!mmcount%\n")
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
      - type: omfile
        file: "${RSYSLOG_OUT_LOG}"
        template: outfmt
YAMLEOF
    add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")'
else
    add_conf '
ruleset(name="countValue") {
    action(type="mmcount" appname="counter" key="!key")
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="outfmt")
}'
fi
add_conf '
if $app-name == "counter" then {
    set $!key = "ab";
    call countValue
    set $!key = "bA";
    call countValue
    set $!key = "ab";
    call countValue
    set $!key = "bA";
    call countValue
}'
startup
injectmsg_literal '<13>1 2026-10-23T00:00:00Z localhost counter - - - trigger'
shutdown_when_empty
wait_shutdown
printf 'ab:1\nbA:1\nab:2\nbA:2\n' > "$RSYSLOG_OUT_LOG.expected"
cmp_exact_file "$RSYSLOG_OUT_LOG.expected" "$RSYSLOG_OUT_LOG"
exit_test
