#!/bin/bash
# Verify native YAML reloadOnHUP=validate parses and privately classifies a
# changed candidate without constructing its action or resolving DNS, and
# leaves the active route unchanged. The same daemon and listener must continue
# writing through the old YAML ruleset.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
generate_conf --yaml-only
sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "validate"' "${TESTCONF_NM}.yaml"
add_yaml_conf 'modules:'
add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
add_yaml_conf '    config.enabled: "on"'
add_yaml_conf 'inputs:'
add_yaml_conf '  - type: imtcp'
add_yaml_conf '    config.enabled: "on"'
add_yaml_conf '    port: "0"'
add_yaml_conf '    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"'
add_yaml_conf '    ruleset: main'
add_yaml_conf '    streamdriver.TlsVerifyDepth: 3'
add_yaml_conf 'rulesets:'
add_yaml_conf '  - name: main'
add_yaml_conf '    actions:'
add_yaml_conf '      - type: omfile'
add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
startup
tcpflood -m1 -i0
wait_queueempty
# The initial source graph and a same-file YAML candidate must produce a
# deterministic no-op report. This proves both frontends reach the shared
# cnfobj/nvlst serializer without changing the active route.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=6 added=0 removed=0 modified=0 invalid=0"* ]]; then
	echo "FAIL: unexpected YAML no-op reload status: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=reuse"* ]]; then
	echo "FAIL: unchanged YAML imtcp profile was not classified reusable: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE" "$CONF_FILE.base"

# An explicit module default must compare equal to the omitted form after
# effective lowering, even though the source graph records a syntax change.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "on"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: explicit YAML imtcp default was not classified reusable: $reload_status"
	error_exit 1
fi

# An input override masks a changed module TLS default in the effective
# listener profile. This exercises string/typed inheritance rather than a raw
# module-structure comparison.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    streamdriver.TlsVerifyDepth: 4' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0 source_capability=reuse"* ]]; then
	echo "FAIL: YAML input TLS override did not mask the module default: $reload_status"
	error_exit 1
fi

# Module defaults use the same private lowering path as input overrides. The
# change is live-capable, but validate remains strictly report-only.
sed '/load: "..\/plugins\/imtcp\/.libs\/imtcp"/a\    flowControl: "off"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: valid YAML imtcp module candidate did not lower report-only: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: changed YAML imtcp module profile was not classified live-capable: $reload_status"
	error_exit 1
fi

# The module load origin is part of the effective source identity. A different
# path must never compare reusable merely because its basename is still imtcp.
sed 's|../plugins/imtcp/.libs/imtcp|./imtcp|' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=1 removed=1 modified=0 invalid=0 source_capability=restart_required"* ]]; then
	echo "FAIL: changed YAML imtcp module origin was classified reusable: $reload_status"
	error_exit 1
fi

# A valid imtcp input change is lowered privately through the module's full
# parameter/default path and classified live-capable. Validate remains
# report-only and cannot alter the established listener or generation.
sed '/ruleset: main/a\    flowControl: "off"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: valid YAML imtcp candidate did not lower report-only: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=live_swap"* ]]; then
	echo "FAIL: changed YAML imtcp input profile was not classified live-capable: $reload_status"
	error_exit 1
fi

# Known parameters with invalid effective values are candidate validation
# failures, not internal daemon errors. maxFrameSize=0 exercises the shared
# startup validator while the listener and generation remain unchanged.
sed '/ruleset: main/a\    maxFrameSize: 0' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_normalization_unsupported active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: invalid YAML imtcp value was misclassified: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=not_evaluated"* ]]; then
	echo "FAIL: invalid YAML imtcp value claimed a capability: $reload_status"
	error_exit 1
fi

# A hostname wildcard is privately materializable without DNS.  Validate must
# classify it live-capable while remaining report-only.
sed '/ruleset: main/a\    allowedSender: ["*.example.invalid"]' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0 source_capability=live_swap"* ]]; then
	echo "FAIL: YAML wildcard ACL was not classified live-capable: $reload_status"
	error_exit 1
fi

# A bare hostname under the default resolving base would call getaddrinfo at
# cold start.  Private reload lowering must not perform that external lookup,
# so the candidate remains a report-only restart boundary.
sed '/ruleset: main/a\    allowedSender: ["localhost"]' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0 source_capability=restart_required"* ]]; then
	echo "FAIL: DNS-resolved YAML ACL did not remain a restart boundary: $reload_status"
	error_exit 1
fi

# Unknown imtcp parameters are syntactically capturable but cannot form an
# effective module snapshot. The module lowerer must reject them before any
# active state changes; this distinguishes semantic lowering from raw diffing.
sed '/ruleset: main/a\    reloadUnknownParameter: "invalid"' "$CONF_FILE.base" >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_scope_unsupported active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unknown YAML imtcp parameter was not rejected by source lowering: $reload_status"
	error_exit 1
fi
if [[ "$reload_status" != *"source_capability=not_evaluated"* ]]; then
	echo "FAIL: unknown YAML imtcp parameter claimed a capability: $reload_status"
	error_exit 1
fi
cp "$CONF_FILE.base" "$CONF_FILE"

sed "s|$RSYSLOG_OUT_LOG|$RSYSLOG2_OUT_LOG|" "$CONF_FILE" >"$CONF_FILE.candidate"
mv "$CONF_FILE.candidate" "$CONF_FILE"
# A changed action is visible in the report, but validate is strictly
# report-only: the established listener continues to use the old route.
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=5 added=0 removed=0 modified=1 invalid=0"* ]]; then
	echo "FAIL: unexpected YAML action-change reload status: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i1
wait_queueempty
# A normal global-object dispatch would create/truncate this file. The YAML
# candidate must be captured without executing that setter.
sed -i '/config.reloadOnHUP:/a\  debug.logFile: "'$RSYSLOG_DYNNAME'.reload-sentinel"' "$CONF_FILE"
cp "$CONF_FILE" "$CONF_FILE.valid"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=4 added=0 removed=0 modified=2 invalid=0"* ]]; then
	echo "FAIL: unexpected reload status: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"
tcpflood -m1 -i2
wait_queueempty

# Exercise parser recovery in the same daemon. A malformed native YAML
# candidate must be rejected, and a later record on the active listener must
# still use the old action. HUP generation and getreloadstatus avoid sleeps.
printf 'version: 2\nglobal: [\n' >"$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=candidate_parse_invalid active_generation=1"* ]]; then
	echo "FAIL: unexpected invalid-candidate status: $reload_status"
	error_exit 1
fi
tcpflood -m1 -i3
wait_queueempty

# Restore the valid YAML candidate to prove parser recovery after the rejected
# document. The active output remains unchanged because validate never commits.
cp "$CONF_FILE.valid" "$CONF_FILE"
issue_HUP
reload_status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$reload_status" != *"result=reported_only active_generation=1 unchanged=4 added=0 removed=0 modified=2 invalid=0"* ]]; then
	echo "FAIL: YAML parser did not recover after invalid candidate: $reload_status"
	error_exit 1
fi
check_file_not_exists "$RSYSLOG_DYNNAME.reload-sentinel"
tcpflood -m1 -i4
wait_queueempty
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000001' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000002' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000003' "$RSYSLOG_OUT_LOG"
content_check 'msgnum:00000004' "$RSYSLOG_OUT_LOG"
check_file_not_exists "$RSYSLOG2_OUT_LOG"
exit_test
