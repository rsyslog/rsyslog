#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A live old dispatch root blocks in an unchanged named action while two private
# shells with a forward reference prepare. Explicit in-progress generation 1
# precedes latch release; generation 2 and exact tagged traffic prove complete
# atomic publication. Subsequent additions, missing-target rollback, metadata,
# action and removal rejection preserve previously active names. YAML runs the
# identical graph through its own frontend. Poll bounds are deadlock guards;
# status, FIFO entry/ack and exact output records are the success oracles.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
require_plugin omprog
if [ "${RULESET_ADDITIONS_YAML:-0}" = 1 ]; then require_yaml_support; fi
mkfifo "$RSYSLOG_DYNNAME.release"
if [ "${RULESET_ADDITIONS_YAML:-0}" = 1 ]; then
    generate_conf --yaml-only
    candidate_file="${TESTCONF_NM}.yaml"
    sed '/debug.abortOnProgramError:/a\
  config.reloadOnHUP: "on"
' "$candidate_file" > "$candidate_file.preamble" || error_exit 1
    # A constant include routes only internal diagnostics; tested rulesets and
    # every candidate remain YAML. Pure-YAML testbench has no .started sink.
    printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_DYNNAME.started" > "$RSYSLOG_DYNNAME.internal.conf"
    printf 'include:\n  - path: "%s.internal.conf"\n' "$RSYSLOG_DYNNAME" >> "$candidate_file.preamble"
else
    generate_conf
    candidate_file="${TESTCONF_NM}.conf"
    cp "$candidate_file" "$candidate_file.preamble"
fi
write_bodies() {
    local stage=$1 bad=${2:-}
    cat <<CONF_EOF
ruleset(name="sink") {
    if \$msg contains "hold" then
        action(type="omprog" name="additions_latch" binary="${srcdir}/testsuites/reload-callgraph-latch.sh $RSYSLOG_DYNNAME.enter $RSYSLOG_DYNNAME.release" confirmMessages="on" confirmTimeout="30000")
    action(type="omfile" name="additions_sink" file="$RSYSLOG_OUT_LOG" template="additionfmt")
}
ruleset(name="router") {
    if \$msg contains "ghost" then set \$.route = "ghost";
    else if \$msg contains "third" then set \$.route = "third";
    else set \$.route = "second";
    if \$msg contains "indirect" then {
        call_indirect \$.route;
    } else {
        call dispatch
    }
}
ruleset(name="dispatch") {
CONF_EOF
    if [ "$stage" = 0 ]; then
        printf '    set $.path = "baseline";\n    call sink\n'
    elif [ "$stage" = 1 ]; then
        printf '    call first\n'
    else
        printf '    call third\n'
    fi
    printf '}\n'
    if [ "$stage" != 0 ]; then
        cat <<'CONF_EOF'
ruleset(name="first") {
    call second
}
ruleset(name="second") {
    set $.path = "second";
    call sink
}
CONF_EOF
    fi
    if [ "$stage" = 2 ]; then
        printf 'ruleset(name="third") {\n    call first\n}\n'
    fi
    case "$bad" in
        retarget) printf 'ruleset(name="ghost") {\n    call sink\n}\n' ;;
        missing) printf 'ruleset(name="ghost") {\n    call missing_private_target\n}\n' ;;
        queue) printf 'ruleset(name="ghost" queue.type="LinkedList") {\n    call sink\n}\n' ;;
        action) printf 'ruleset(name="ghost") {\n    action(type="omfile" name="new_forbidden_sink" file="%s")\n}\n' "$RSYSLOG2_OUT_LOG" ;;
        duplicate) printf 'ruleset(name="ghost") {\n    call sink\n}\nruleset(name="GHOST") {\n    call sink\n}\n' ;;
    esac
}
write_config() {
    local stage=$1 bad=${2:-} input_target=router
    if [ "$bad" = retarget ]; then input_target=ghost; fi
    cat "$candidate_file.preamble" > "$candidate_file.next"
    if [ "${RULESET_ADDITIONS_YAML:-0}" = 1 ]; then
        cat >> "$candidate_file.next" <<YAML_EOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
  - load: "../plugins/omprog/.libs/omprog"
templates:
  - name: additionfmt
    type: string
    string: "%msg% path=%\$.path%\n"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: "$input_target"
rulesets:
YAML_EOF
        write_bodies "$stage" "$bad" | awk '
            /^ruleset\(name=/ {
                name=$0; sub(/^ruleset\(name="/, "", name); sub(/".*/, "", name);
                print "  - name: " name;
                if ($0 ~ /queue.type=/) print "    queue.type: LinkedList";
                print "    script: |"; next
            }
            /^}$/ {next}
            { print "      " $0 }' >> "$candidate_file.next"
    else
        cat >> "$candidate_file.next" <<CONF_EOF
global(config.reloadOnHUP="on")
module(load="../plugins/imtcp/.libs/imtcp")
module(load="../plugins/omprog/.libs/omprog")
template(name="additionfmt" type="string" string="%msg% path=%\$.path%\n")
input(type="imtcp" port="0" listenPortFileName="$RSYSLOG_DYNNAME.tcpflood_port" ruleset="$input_target")
CONF_EOF
        write_bodies "$stage" "$bad" >> "$candidate_file.next"
    fi
    mv "$candidate_file.next" "$candidate_file"
}
status() { echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT"; }
await_status() {
    local expected=$1 response
    for ((attempt=0; attempt<200; ++attempt)); do
        response=$(status)
        if [[ "$response" == *"$expected"* ]]; then return; fi
        "$TESTTOOL_DIR/msleep" 20
    done
    echo "FAIL: wanted $expected; got $response"; error_exit 1
}
send_record() { printf '<167>Mar 10 01:00:00 host app: %s\n' "$1" >&9 || error_exit 1; }
write_config 0
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
send_record record-old-hold
wait_file_exists "$RSYSLOG_DYNNAME.enter"
write_config 1
kill -HUP "$(cat "$RSYSLOG_PIDBASE.pid")"
await_status 'result=in_progress active_generation=1'
printf 'release\n' > "$RSYSLOG_DYNNAME.release"
await_status 'result=activated active_generation=2'
wait_content 'record-old-hold path=baseline' "$RSYSLOG_OUT_LOG"
send_record record-new-static
send_record record-new-indirect
wait_content 'record-new-indirect path=second' "$RSYSLOG_OUT_LOG"
write_config 2
issue_HUP
await_status 'result=activated active_generation=3'
send_record record-later-static
send_record record-later-indirect-third
wait_content 'record-later-indirect-third path=second' "$RSYSLOG_OUT_LOG"
for bad in missing queue action duplicate retarget; do
    write_config 2 "$bad"
    issue_HUP
    response=$(status)
    [[ "$response" == *'active_generation=3'* && "$response" == *'result=candidate_'* ]] || {
        echo "FAIL: $bad addition escaped private rejection: $response"; error_exit 1;
    }
done
# Previously published names may not be removed, even as part of a larger plan.
write_config 0
issue_HUP
await_status 'result=candidate_scope_unsupported active_generation=3'
write_config 2
send_record record-aborted-indirect-ghost
send_record record-retained-indirect-third
wait_content 'record-retained-indirect-third path=second' "$RSYSLOG_OUT_LOG"
exec 9>&-
shutdown_when_empty
wait_shutdown
for record in record-new-static record-new-indirect record-later-static record-later-indirect-third record-retained-indirect-third; do
    content_count_check "$record path=second" 1 "$RSYSLOG_OUT_LOG"
done
content_count_check 'record-old-hold path=baseline' 1 "$RSYSLOG_OUT_LOG"
custom_assert_content_missing 'record-aborted-indirect-ghost' "$RSYSLOG_OUT_LOG"
content_check "ruleset 'ghost' cannot be found" "$RSYSLOG_DYNNAME.started"
exit_test
