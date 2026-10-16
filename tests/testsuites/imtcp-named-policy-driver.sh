#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Two persistent A consumers retain independent local buckets across a B-policy
# change and unrelated profile/ACL changes. Replacing A gives both consumers
# fresh local buckets backed by one policy configuration. Adding/selecting C,
# temporarily unbinding/rebinding A (fresh local buckets), removing A,
# severity replacement and malformed/complex rejection retain all connections.
# Debug frames consume tokens; an info marker bypasses severity=debug on the
# same stream and proves preceding dropped frames were processed. No timing
# threshold or stderr diagnostic is used as the negative-delivery oracle.
# Repeating each committed candidate as a no-op proves retirement completes.
# An ACL-only update on both A consumers must preserve their exhausted local
# budget; an info barrier on the same stream proves the denied debug frame ran.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    require_yaml_support
    generate_conf --yaml-only
    sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
    add_yaml_conf 'ratelimits:
  - name: A
    interval: "3600"
    burst: "3"
    severity: debug
  - name: B
    interval: "0"
    burst: "100"
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"
    name: first
    ratelimit.name: A
    ruleset: main
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.second_port"
    name: second
    ratelimit.name: A
    ruleset: main
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.third_port"
    name: third
    ratelimit.name: B
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: named_sink
        file: "'$RSYSLOG_OUT_LOG'"'
else
    generate_conf
    add_conf '
global(config.reloadOnHUP="on")
ratelimit(name="A" interval="3600" burst="3" severity="debug")
ratelimit(name="B" interval="0" burst="100")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" name="first" ratelimit.name="A" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.second_port" name="second" ratelimit.name="A" ruleset="main")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.third_port" name="third" ratelimit.name="B" ruleset="main")
ruleset(name="main") { action(type="omfile" name="named_sink" file="'$RSYSLOG_OUT_LOG'") }
'
fi
startup
second_port=''
third_port=''
assign_file_content second_port "$RSYSLOG_DYNNAME.second_port"
assign_file_content third_port "$RSYSLOG_DYNNAME.third_port"
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
exec 9<>"/dev/tcp/127.0.0.1/$second_port"
exec 10<>"/dev/tcp/127.0.0.1/$third_port"
generation=1
named_status() {
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
}
named_publish() {
    issue_HUP
    generation=$((generation + 1))
    named_status
    [[ "$reload_status" == *"result=activated active_generation=$generation "* ]] || error_exit 1
    issue_HUP
    named_status
    [[ "$reload_status" == *"result=reported_only active_generation=$generation "* ]] || error_exit 1
}
named_send() {
    printf '<%s>Mar 10 01:00:00 host app: %s\n' "$2" "$3" >&"$1" || error_exit 1
}
named_expect_exhausted() {
    local prefix=$1 accepted=${2:-166} rejected=${3:-167} descriptor
    # A same-stream higher-severity marker acknowledges each negative probe.
    for descriptor in 8 9; do
        named_send "$descriptor" "$rejected" "$prefix-$descriptor-must-drop"
        named_send "$descriptor" "$accepted" "$prefix-$descriptor-barrier"
        wait_content "$prefix-$descriptor-barrier" "$RSYSLOG_OUT_LOG"
        check_not_present "$prefix-$descriptor-must-drop" "$RSYSLOG_OUT_LOG"
    done
}
named_send 8 167 a-baseline-1
named_send 9 167 a-baseline-2
named_send 8 167 a-baseline-first-extra
named_send 9 167 a-baseline-second-extra
wait_content a-baseline-2 "$RSYSLOG_OUT_LOG"
wait_content a-baseline-first-extra "$RSYSLOG_OUT_LOG"
wait_content a-baseline-second-extra "$RSYSLOG_OUT_LOG"
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i 's/burst: "100"/burst: "101"/' "$CONF_FILE"
else
    sed -i 's/burst="100"/burst="101"/' "$CONF_FILE"
fi
named_publish
named_send 8 167 a-last-token
named_send 9 167 a-second-last-token
wait_content a-last-token "$RSYSLOG_OUT_LOG"
wait_content a-second-last-token "$RSYSLOG_OUT_LOG"
named_send 9 167 a-must-drop
named_send 9 166 a-drop-barrier
wait_content a-drop-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-must-drop "$RSYSLOG_OUT_LOG"
named_expect_exhausted b-change-retained
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/name: third/a\    defaultTZ: "+01:00"' "$CONF_FILE"
else
    sed -i 's/name="third"/name="third" defaultTZ="+01:00"/' "$CONF_FILE"
fi
named_publish
named_send 8 167 a-still-exhausted
named_send 8 166 a-profile-barrier
wait_content a-profile-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-still-exhausted "$RSYSLOG_OUT_LOG"
named_expect_exhausted profile-retained
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/ratelimit[.]name: A/a\    allowedSender: ["127.0.0.0/8"]' "$CONF_FILE"
else
    sed -i '/^input(/s/ratelimit[.]name="A"/ratelimit.name="A" allowedSender=["127.0.0.0\/8"]/' "$CONF_FILE"
fi
named_publish
named_send 9 167 a-acl-must-drop
named_send 9 166 a-acl-barrier
wait_content a-acl-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-acl-must-drop "$RSYSLOG_OUT_LOG"
named_expect_exhausted acl-retained
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i 's/burst: "3"/burst: "2"/' "$CONF_FILE"
else
    sed -i 's/burst="3"/burst="2"/' "$CONF_FILE"
fi
named_publish
named_send 8 167 a-new-1
named_send 9 167 a-new-2
named_send 8 167 a-new-first-last
named_send 9 167 a-new-second-last
wait_content a-new-2 "$RSYSLOG_OUT_LOG"
wait_content a-new-first-last "$RSYSLOG_OUT_LOG"
wait_content a-new-second-last "$RSYSLOG_OUT_LOG"
named_send 8 167 a-new-must-drop
named_send 8 166 a-new-barrier
wait_content a-new-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-new-must-drop "$RSYSLOG_OUT_LOG"
named_expect_exhausted changed-policy-local
cp "$CONF_FILE" "$CONF_FILE.bound"
# Unchanged A remains declared while temporarily unused. Its registry owner is
# retained (native identity oracle); rebinding creates normal fresh local buckets.
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/ratelimit.name: A/d' "$CONF_FILE"
else
    sed -i 's/ ratelimit.name="A"//g' "$CONF_FILE"
fi
named_publish
cp "$CONF_FILE.bound" "$CONF_FILE"
named_publish
named_send 8 167 a-rebind-first-1
named_send 8 167 a-rebind-first-2
named_send 9 167 a-rebind-second-1
named_send 9 167 a-rebind-second-2
wait_content a-rebind-first-2 "$RSYSLOG_OUT_LOG"
wait_content a-rebind-second-2 "$RSYSLOG_OUT_LOG"
named_send 9 167 a-rebind-must-drop
named_send 9 166 a-rebind-barrier
wait_content a-rebind-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-rebind-must-drop "$RSYSLOG_OUT_LOG"
named_expect_exhausted rebound-local
# Replace severity alone: info bypasses a severity=debug bucket, but is limited
# after severity=info. The candidate starts a fresh local bucket for each consumer.
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i 's/severity: debug/severity: info/' "$CONF_FILE"
else
    sed -i 's/severity="debug"/severity="info"/' "$CONF_FILE"
fi
named_publish
named_send 8 166 a-info-1
named_send 9 166 a-info-2
named_send 8 166 a-info-first-last
named_send 9 166 a-info-second-last
wait_content a-info-2 "$RSYSLOG_OUT_LOG"
wait_content a-info-first-last "$RSYSLOG_OUT_LOG"
wait_content a-info-second-last "$RSYSLOG_OUT_LOG"
named_send 8 166 a-info-must-drop
named_send 8 165 a-info-barrier
wait_content a-info-barrier "$RSYSLOG_OUT_LOG"
check_not_present a-info-must-drop "$RSYSLOG_OUT_LOG"
named_expect_exhausted severity-local 165 166
cp "$CONF_FILE" "$CONF_FILE.valid"
for invalid in malformed complex; do
    cp "$CONF_FILE.valid" "$CONF_FILE"
    if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
        if [[ "$invalid" == malformed ]]; then
            sed -i 's/severity: info/severity: not-a-severity/' "$CONF_FILE"
        else
            sed -i '/severity: info/a\    perSource.maxStates: "5"' "$CONF_FILE"
        fi
    else
        if [[ "$invalid" == malformed ]]; then
            sed -i 's/severity="info"/severity="not-a-severity"/' "$CONF_FILE"
        else
            sed -i 's/severity="info"/severity="info" perSource.maxStates="5"/' "$CONF_FILE"
        fi
    fi
    issue_HUP
    named_status
    [[ "$reload_status" == *"active_generation=$generation "* && "$reload_status" != *"result=activated "* ]] || error_exit 1
    named_send 10 165 "$invalid-retained"
    wait_content "$invalid-retained" "$RSYSLOG_OUT_LOG"
done
cp "$CONF_FILE.valid" "$CONF_FILE"
# Remove A atomically with its last bindings; B remains the same owner.
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/  - name: A/,+3d; s/ratelimit.name: A/ratelimit.name: B/g' "$CONF_FILE"
else
    sed -i '/^ratelimit(name="A"/d; s/ratelimit.name="A"/ratelimit.name="B"/g' "$CONF_FILE"
fi
named_publish
named_send 8 167 removed-a-first
named_send 9 167 removed-a-second
wait_content removed-a-first "$RSYSLOG_OUT_LOG"
wait_content removed-a-second "$RSYSLOG_OUT_LOG"
# Add/select C in the same generation, then return to unchanged B while
# removing C. Existing B consumers must keep sharing their original owner.
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/^ratelimits:/a\  - name: C\n    interval: "3600"\n    burst: "1"\n    severity: debug' "$CONF_FILE"
    sed -i '/name: first/,/ruleset: main/s/ratelimit.name: B/ratelimit.name: C/' "$CONF_FILE"
else
    sed -i '/^ratelimit(name="B"/a\ratelimit(name="C" interval="3600" burst="1" severity="debug")' "$CONF_FILE"
    sed -i '/name="first"/s/ratelimit.name="B"/ratelimit.name="C"/' "$CONF_FILE"
fi
named_publish
named_send 8 167 added-c-first
wait_content added-c-first "$RSYSLOG_OUT_LOG"
named_send 8 167 added-c-must-drop
named_send 8 166 added-c-barrier
wait_content added-c-barrier "$RSYSLOG_OUT_LOG"
check_not_present added-c-must-drop "$RSYSLOG_OUT_LOG"
if [[ "$RSYSLOG_NAMED_POLICY_FORMAT" == yaml ]]; then
    sed -i '/  - name: C/,+3d; s/ratelimit.name: C/ratelimit.name: B/g' "$CONF_FILE"
else
    sed -i '/^ratelimit(name="C"/d; s/ratelimit.name="C"/ratelimit.name="B"/g' "$CONF_FILE"
fi
named_publish
named_send 8 167 selected-b-again
wait_content selected-b-again "$RSYSLOG_OUT_LOG"
exec 8>&-
exec 9>&-
exec 10>&-
shutdown_when_empty
wait_shutdown
exit_test
