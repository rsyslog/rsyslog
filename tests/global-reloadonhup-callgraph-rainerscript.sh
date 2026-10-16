#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Two unchanged caller queues execute old callee bodies concurrently. Named
# synchronous omprog actions publish entry and withhold acknowledgement on FIFO
# latches. Generation must stay old after releasing only one caller; both new
# roots publish together after the second releases. Exact old/new output tags
# prove retained execution and unchanged static/indirect caller routing. Bounded
# polling only awaits explicit status/files, never establishes absence by time.
# Missing targets and real fence timeout preserve generation/action ownership;
# retry then proves the privately bound nested call remains executable.
. ${srcdir:=.}/diag.sh init
require_plugin omprog
if [ "${CALLGRAPH_YAML:-0}" = 1 ]; then require_yaml_support; require_plugin imtcp; fi
for caller in 1 2; do mkfifo "$RSYSLOG_DYNNAME.release$caller"; done
if [ "${CALLGRAPH_YAML:-0}" = 1 ]; then generate_conf --yaml-only; else generate_conf; fi
# Rewrite beside the unique candidate and publish only successful sed output.
# The append script below uses POSIX a\ syntax so BSD/macOS sed also accepts it.
rewrite_candidate() {
    local candidate_tmp="${candidate_file}.sed-tmp"
    if ! sed "$@" "$candidate_file" > "$candidate_tmp"; then
        rm -f "$candidate_tmp"
        error_exit 1
    fi
    mv "$candidate_tmp" "$candidate_file" || error_exit 1
}

write_rulesets() {
cat <<CONF_EOF
ruleset(name="empty") {
}
ruleset(name="callee1") {
    if \$msg contains "route" then {
        call empty
    }
    set \$.version = "old";
    if \$msg contains "hold" then
        action(type="omprog" name="latch1" binary="${srcdir}/testsuites/reload-callgraph-latch.sh $RSYSLOG_DYNNAME.enter1 $RSYSLOG_DYNNAME.release1" confirmMessages="on" confirmTimeout="30000")
    action(type="omfile" name="sink1" file="$RSYSLOG_OUT_LOG" template="callfmt")
}
ruleset(name="callee2") {
    set \$.version = "old";
    if \$msg contains "hold" then
        action(type="omprog" name="latch2" binary="${srcdir}/testsuites/reload-callgraph-latch.sh $RSYSLOG_DYNNAME.enter2 $RSYSLOG_DYNNAME.release2" confirmMessages="on" confirmTimeout="30000")
    action(type="omfile" name="sink2" file="$RSYSLOG_OUT_LOG" template="callfmt")
}
ruleset(name="queued" queue.type="LinkedList") {
    action(type="omfile" name="queued_sink" file="$RSYSLOG2_OUT_LOG" template="callfmt")
}
ruleset(name="caller1" queue.type="LinkedList") {
    if \$msg contains "route1" then {
        call callee1
    }
    call queued
}
ruleset(name="caller2" queue.type="LinkedList") {
    call_indirect "callee2";
    call queued
}
CONF_EOF
}
if [ "${CALLGRAPH_YAML:-0}" = 1 ]; then
    candidate_file="${TESTCONF_NM}.yaml"
    rewrite_candidate '/debug.abortOnProgramError:/a\
  config.reloadOnHUP: "on"
'
    cat >> "$candidate_file" <<YAML_EOF
modules:
  - load: "../plugins/omprog/.libs/omprog"
  - load: "../plugins/imtcp/.libs/imtcp"
templates:
  - name: callfmt
    type: string
    string: '%msg% version=%\$.version%\n'
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: main
rulesets:
  - name: main
    script: |
      if \$msg contains "route1" then call caller1
      if \$msg contains "route2" then call caller2
YAML_EOF
    write_rulesets | awk '
        /^ruleset\(name=/ {
            name=$0; sub(/^ruleset\(name="/, "", name); sub(/".*/, "", name);
            print "  - name: " name;
            if ($0 ~ /queue.type=/) print "    queue.type: LinkedList";
            print "    script: |"; next
        }
        /^}$/ {next}
        { print "      " $0 }' >> "$candidate_file"
else
    add_conf '
global(config.reloadOnHUP="on")
module(load="../plugins/omprog/.libs/omprog")
template(name="callfmt" type="string" string="%msg% version=%$.version%\n")
'
    write_rulesets >> "${TESTCONF_NM}.conf"
    candidate_file="${TESTCONF_NM}.conf"
    add_conf '
if $msg contains "route1" then call caller1
if $msg contains "route2" then call caller2
'
fi
startup
send_record() {
    if [ "${CALLGRAPH_YAML:-0}" = 1 ]; then
        printf '%s\n' "$1" >"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
    else
        injectmsg_literal "$1"
    fi
}
send_record '<167>Mar 10 01:00:00 host app: route1-hold'
send_record '<167>Mar 10 01:00:00 host app: route2-hold'
wait_file_exists "$RSYSLOG_DYNNAME.enter1"
wait_file_exists "$RSYSLOG_DYNNAME.enter2"
rewrite_candidate 's/"old"/"new"/g'
status() { echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT"; }
await_status() {
    local expected=$1 response
    for ((attempt=0; attempt<500; ++attempt)); do
        response=$(status)
        if [[ "$response" == *"$expected"* ]]; then return; fi
        "$TESTTOOL_DIR/msleep" 20
    done
    echo "FAIL: wanted $expected; got $response"; error_exit 1
}
kill -HUP "$(cat "$RSYSLOG_PIDBASE.pid")"
await_status 'result=in_progress active_generation=1'
printf 'release\n' > "$RSYSLOG_DYNNAME.release1"
wait_content 'route1-hold version=old' "$RSYSLOG_OUT_LOG"
response=$(status)
[[ "$response" == *'result=in_progress active_generation=1'* ]] || { echo "$response"; error_exit 1; }
printf 'release\n' > "$RSYSLOG_DYNNAME.release2"
await_status 'result=activated active_generation=2'
send_record '<167>Mar 10 01:00:00 host app: route1-after'
send_record '<167>Mar 10 01:00:00 host app: route2-after'
wait_content 'route1-after version=new' "$RSYSLOG_OUT_LOG"
wait_content 'route2-after version=new' "$RSYSLOG_OUT_LOG"
# A missing static target must fail during private preparation and leave both
# active roots and action owners intact.
cp "$candidate_file" "$candidate_file.good"
rewrite_candidate 's/call callee1/call missing_callee/'
issue_HUP
response=$(status)
[[ "$response" == *'result=candidate_scope_unsupported active_generation=2'* ]] || { echo "$response"; error_exit 1; }
cp "$candidate_file.good" "$candidate_file"

# Hold both old bodies through the real five-second production fence deadline.
# Explicit activation_failed, rather than elapsed time, proves the abort. The
# old action owners remain usable after the private candidate is discarded.
rm "$RSYSLOG_DYNNAME.enter1" "$RSYSLOG_DYNNAME.enter2"
send_record '<167>Mar 10 01:00:00 host app: route1-timeout-hold'
send_record '<167>Mar 10 01:00:00 host app: route2-timeout-hold'
wait_file_exists "$RSYSLOG_DYNNAME.enter1"
wait_file_exists "$RSYSLOG_DYNNAME.enter2"
rewrite_candidate 's/"new"/"final"/g'
kill -HUP "$(cat "$RSYSLOG_PIDBASE.pid")"
await_status 'result=in_progress active_generation=2'
await_status 'result=activation_failed active_generation=2'
printf 'release\n' > "$RSYSLOG_DYNNAME.release1"
printf 'release\n' > "$RSYSLOG_DYNNAME.release2"
wait_content 'route1-timeout-hold version=new' "$RSYSLOG_OUT_LOG"
wait_content 'route2-timeout-hold version=new' "$RSYSLOG_OUT_LOG"
issue_HUP
response=$(status)
[[ "$response" == *'result=activated active_generation=3'* ]] || { echo "$response"; error_exit 1; }
send_record '<167>Mar 10 01:00:00 host app: route1-final'
send_record '<167>Mar 10 01:00:00 host app: route2-final'
wait_content 'route1-final version=final' "$RSYSLOG_OUT_LOG"
wait_content 'route2-final version=final' "$RSYSLOG_OUT_LOG"
shutdown_when_empty
wait_shutdown
content_count_check 'route1-hold version=old' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route2-hold version=old' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route1-after version=new' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route2-after version=new' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route1-timeout-hold version=new' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route2-timeout-hold version=new' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route1-final version=final' 1 "$RSYSLOG_OUT_LOG"
content_count_check 'route2-final version=final' 1 "$RSYSLOG_OUT_LOG"
for record in route1-hold route2-hold route1-after route2-after \
    route1-timeout-hold route2-timeout-hold route1-final route2-final; do
    content_count_check "$record version=" 1 "$RSYSLOG2_OUT_LOG"
done
content_count_check 'route' 8 "$RSYSLOG2_OUT_LOG"
exit_test
