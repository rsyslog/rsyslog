#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Native YAML notification policy must publish at the live fence. A newly
# accepted peer and its normal EOF must each emit one daemon diagnostic,
# observed through a constant omfile include, while an existing stream remains
# usable. Restoring defaults suppresses later notifications; final shutdown
# drains internal messages before exact counts, without timing-based absence.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"\n  processInternalMessages: "on"' "${TESTCONF_NM}.yaml"
printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_DYNNAME.internal" >"$RSYSLOG_DYNNAME.internal.conf"
add_yaml_conf 'include:'
add_yaml_conf '  - path: "'$RSYSLOG_DYNNAME'.internal.conf"'
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    name: notifications'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
exec 9<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: notification-before\n' >&9 || error_exit 1
wait_content 'notification-before' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE" "$CONF_FILE.base"
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    notifyOnConnectionOpen: "on"\n    notifyOnConnectionClose: "on"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
[[ "$reload_status" == *"result=activated active_generation=2"* &&
   "$reload_status" == *"source_capability=live_swap"* &&
   "$reload_status" == *"retirement_pending=0 retirement_last_error=0 retirement_retry_count=0"* ]] || error_exit 1 "$reload_status"
exec 8<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
wait_content 'imtcp: connection established with host:' "$RSYSLOG_DYNNAME.internal"
printf '<167>Mar 10 01:00:00 host app: notification-new\n' >&8 || error_exit 1
wait_content 'notification-new' "$RSYSLOG_OUT_LOG"
exec 8>&-
wait_content 'closed by remote peer' "$RSYSLOG_DYNNAME.internal"
printf '<167>Mar 10 01:00:00 host app: notification-retained\n' >&9 || error_exit 1
wait_content 'notification-retained' "$RSYSLOG_OUT_LOG"
cp "$CONF_FILE.base" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
[[ "$reload_status" == *"result=activated active_generation=3"* ]] || error_exit 1 "$reload_status"
exec 7<>"/dev/tcp/127.0.0.1/$TCPFLOOD_PORT"
printf '<167>Mar 10 01:00:00 host app: notification-disabled\n' >&7 || error_exit 1
wait_content 'notification-disabled' "$RSYSLOG_OUT_LOG"
exec 7>&-
exec 9>&-
shutdown_when_empty
wait_shutdown
content_count_check 'imtcp: connection established with host:' 1 "$RSYSLOG_DYNNAME.internal"
content_count_check 'closed by remote peer' 1 "$RSYSLOG_DYNNAME.internal"
exit_test
