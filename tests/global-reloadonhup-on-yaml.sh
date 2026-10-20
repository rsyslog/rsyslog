#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify native YAML reloadOnHUP=on fails closed in the Release-B foundation.
# The candidate changes only the omfile target; because activation is not yet
# safe, both numbered records must use the old active ruleset after HUP.
# An impstats record with on_total=1 proves on-policy rejection/accounting;
# wait_content synchronizes that scrape without a fixed timing assumption.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
require_plugin impstats
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
sed -i '/debug.abortOnProgramError:/a\  processInternalMessages: "on"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '  - load: "../plugins/impstats/.libs/impstats"'
add_yaml_conf '    interval: "1"'
add_yaml_conf '    log.syslog: "off"'
add_yaml_conf '    log.file: "'$RSYSLOG_DYNNAME'.policy.log"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -i0
wait_queueempty
sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
issue_HUP
wait_content 'reload_on_total=1' "${RSYSLOG_DYNNAME}.policy.log"
tcpflood -m1 -i1
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
content_check 'reload_on_total=1' "${RSYSLOG_DYNNAME}.policy.log"
content_check 'reload_on_rejected_total=1' "${RSYSLOG_DYNNAME}.policy.log"
content_check 'reload_legacy_hook_total=1' "${RSYSLOG_DYNNAME}.policy.log"
exit_test
