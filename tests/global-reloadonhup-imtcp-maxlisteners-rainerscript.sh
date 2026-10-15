#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Resize retained imtcp listener tables independently of maxSessions. Module
# growth and input shrink must retain both an established peer and fresh
# accepts, advance generation only on activation, and reuse an identical HUP.
# A candidate combining growth with an unsupported worker-count change rejects
# atomically; the later supported growth proves that rejection did not publish
# its source baseline. An invalid regex then fails after array preparation,
# proving abort keeps the old tables/traffic intact before a supported retry.
# Native helper tests cover occupied-tail shrink and each
# allocation failure; the IPv4-only integration has exactly one listener socket.
# Validate wrappers require the same capability without changing generation.
# Completed HUP/status plus exact old/new-peer records are the oracles; no
# elapsed-time or socket-handshake-only check is treated as delivery success.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
MODE="${RSYSLOG_RELOAD_ENDPOINT_MODE:-on}"
FORMAT="${RSYSLOG_RELOAD_CONFIG_FORMAT:-rainerscript}"
case "$MODE" in on|validate) ;; *) error_exit 1 ;; esac
case "$FORMAT" in rainerscript|yaml) ;; *) error_exit 1 ;; esac
if [[ "$FORMAT" == yaml ]]; then
    require_yaml_support
    generate_conf --yaml-only
    sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "'"$MODE"'"' "$TESTCONF_NM.yaml"
    add_yaml_conf 'modules:
  - load: "../plugins/imtcp/.libs/imtcp"
    maxListeners: 4
    workerThreads: 1
inputs:
  - type: imtcp
    address: 127.0.0.1
    port: "0"
    listenPortFileName: "'"$RSYSLOG_DYNNAME"'.tcpflood_port"
    name: capacity
    ruleset: main
rulesets:
  - name: main
    actions:
      - type: omfile
        name: sink
        file: "'"$RSYSLOG_OUT_LOG"'"
'
else
    generate_conf
    add_conf 'global(config.reloadOnHUP="'"$MODE"'")
module(load="../plugins/imtcp/.libs/imtcp" maxListeners="4" workerThreads="1")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'"$RSYSLOG_DYNNAME"'.tcpflood_port" name="capacity" ruleset="main")
ruleset(name="main") {
  action(type="omfile" name="sink" file="'"$RSYSLOG_OUT_LOG"'")
}
'
fi
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1
assert_traffic() {
    local tag="$1"
    printf '<167>Mar 10 01:00:00 host app: retained-%s\n' "$tag" >&9 || error_exit 1
    wait_content "retained-$tag" "$RSYSLOG_OUT_LOG"
    exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT" || error_exit 1
    printf '<167>Mar 10 01:00:00 host app: new-%s\n' "$tag" >&8 || error_exit 1
    wait_content "new-$tag" "$RSYSLOG_OUT_LOG"
    exec 8>&-
}
generation=1
expect_supported() {
    local tag="$1"
    local result=reported_only
    [[ "$MODE" == on ]] && { result=activated; generation=$((generation + 1)); }
    issue_HUP
    local status
    status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$status" != *"result=$result active_generation=$generation"* ||
          "$status" != *"source_capability=live_swap"* ]]; then
        echo "FAIL: maxListeners $tag: $status"
        error_exit 1
    fi
    assert_traffic "$tag"
}
cp "$CONF_FILE" "$CONF_FILE.startup"
if [[ "$FORMAT" == yaml ]]; then
    sed 's/maxListeners: 4/maxListeners: 6/' "$CONF_FILE.startup" >"$CONF_FILE.grown"
else
    sed 's/maxListeners="4"/maxListeners="6"/' "$CONF_FILE.startup" >"$CONF_FILE.grown"
fi
cp "$CONF_FILE.grown" "$CONF_FILE"
expect_supported module-growth
if [[ "$FORMAT" == yaml ]]; then
    sed '/    ruleset: main/a\    maxListeners: 1' "$CONF_FILE.grown" >"$CONF_FILE.shrunk"
else
    sed 's/ruleset="main")/maxListeners="1" ruleset="main")/' "$CONF_FILE.grown" >"$CONF_FILE.shrunk"
fi
cp "$CONF_FILE.shrunk" "$CONF_FILE"
expect_supported input-shrink
if [[ "$MODE" == on ]]; then
    issue_HUP
    status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    [[ "$status" == *"result=reported_only active_generation=$generation"* &&
       "$status" == *"source_capability=reuse"* ]] || error_exit 1
fi
if [[ "$FORMAT" == yaml ]]; then
    sed 's/maxListeners: 1/maxListeners: 3/' "$CONF_FILE.shrunk" >"$CONF_FILE.retry"
    sed 's/workerThreads: 1/workerThreads: 2/' "$CONF_FILE.retry" >"$CONF_FILE"
else
    sed 's/maxListeners="1"/maxListeners="3"/' "$CONF_FILE.shrunk" >"$CONF_FILE.retry"
    sed 's/workerThreads="1"/workerThreads="2"/' "$CONF_FILE.retry" >"$CONF_FILE"
fi
issue_HUP
status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
result=candidate_scope_unsupported
[[ "$MODE" == validate ]] && result=reported_only
if [[ "$status" != *"result=$result active_generation=$generation"* ||
      "$status" != *"source_capability=restart_required"* ]]; then
    echo "FAIL: unsupported worker change published listener capacity: $status"
    error_exit 1
fi
assert_traffic rollback
if [[ "$MODE" == on ]]; then
    # Regex prevalidation follows listener-array allocation in Prepare, so
    # this failure exercises destruction of all three unpublished arrays.
    if [[ "$FORMAT" == yaml ]]; then
        sed '/    ruleset: main/a\    framing.delimiter.regex: "["' "$CONF_FILE.retry" >"$CONF_FILE"
    else
        sed 's/ruleset="main")/framing.delimiter.regex="[" ruleset="main")/' "$CONF_FILE.retry" >"$CONF_FILE"
    fi
    issue_HUP
    status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$status" != *"result=activation_failed active_generation=$generation"* ||
          "$status" != *"source_capability=live_and_new_sessions"* ]]; then
        echo "FAIL: listener-table Prepare failure did not roll back: $status"
        error_exit 1
    fi
    assert_traffic prepare-abort
fi
cp "$CONF_FILE.retry" "$CONF_FILE"
expect_supported input-growth-after-rollback
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
