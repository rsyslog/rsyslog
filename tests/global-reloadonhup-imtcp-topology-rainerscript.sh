#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Topology must be rejected before prepare in both on and validate modes.
# Add/remove/replace attempts retain generation one and the established stream's
# routing. Absent candidate port files prove no prepared socket was published;
# acknowledged status plus output markers avoid timing-based negative oracles.
# Require the exact on-mode scope rejection, or validate-mode report-only result,
# so parser errors cannot satisfy the topology oracle. Replacement changes the
# actual address tuple to another valid loopback address, not just metadata.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
mode="${RELOAD_TOPOLOGY_MODE:-on}"
expected_result=candidate_scope_unsupported
[[ "$mode" == validate ]] && expected_result=reported_only
generate_conf
add_conf 'global(config.reloadOnHUP="'$mode'")'
add_conf 'module(load="../plugins/imtcp/.libs/imtcp")'
add_conf 'input(type="imtcp" name="original" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port" ruleset="main")'
add_conf 'ruleset(name="main") { action(type="omfile" name="sink" file="'$RSYSLOG_OUT_LOG'") }'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
cp "$CONF_FILE" "$CONF_FILE.base"
for change in add remove replace; do
    case "$change" in
        add)
            cp "$CONF_FILE.base" "$CONF_FILE"
            printf 'input(type="imtcp" name="added" port="0" listenPortFileName="%s.added.port" ruleset="main")\n' "$RSYSLOG_DYNNAME" >>"$CONF_FILE"
            ;;
        remove)
            sed 's/input(type="imtcp"[^)]*)//' "$CONF_FILE.base" >"$CONF_FILE"
            ;;
        replace)
            sed -e 's/address="127.0.0.1"/address="127.0.0.2"/' \
                -e "s|$RSYSLOG_DYNNAME.tcpflood_port|$RSYSLOG_DYNNAME.replacement.port|" "$CONF_FILE.base" >"$CONF_FILE"
            ;;
    esac
    issue_HUP
    reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$reload_status" != *"active_generation=1"* ||
          "$reload_status" != *"source_capability=restart_required"* ||
          "$reload_status" != *"result=$expected_result "* ]]; then
        echo "FAIL: $mode $change topology was not rejected: $reload_status"
        error_exit 1
    fi
    [[ ! -e "$RSYSLOG_DYNNAME.added.port" && ! -e "$RSYSLOG_DYNNAME.replacement.port" ]] || error_exit 1
    printf '<167>Mar 10 01:00:00 host app: topology-survives-%s\n' "$change" >&9 || error_exit 1
    wait_content "topology-survives-$change" "$RSYSLOG_OUT_LOG"
done
exec 9>&-
shutdown_when_empty
wait_shutdown
exit_test
