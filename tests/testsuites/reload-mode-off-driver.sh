# SPDX-License-Identifier: Apache-2.0
# shellcheck shell=bash
# A committed on-to-off scalar controls the next HUP. Replacing source with on
# cannot self-enable an off controller: completion/status and generation are the
# oracle, followed by normal message delivery and synchronized shutdown.
. ${srcdir:=.}/diag.sh init
generate_conf ${RELOAD_MODE_YAML:+--yaml-only}
if test -n "${RELOAD_MODE_YAML:-}"; then
    require_yaml_support
    sed -i '/debug.abortOnProgramError:/a\  config.reloadOnHUP: "on"' "${TESTCONF_NM}.yaml"
    printf 'action(type="omfile" file="%s")\n' "$RSYSLOG_OUT_LOG" >"$RSYSLOG_DYNNAME.internal.conf"
    cat >>"${TESTCONF_NM}.yaml" <<YAML_EOF
include:
  - path: "$RSYSLOG_DYNNAME.internal.conf"
YAML_EOF
    mode_on='config.reloadOnHUP: "on"'
    mode_off='config.reloadOnHUP: "off"'
else
    add_conf 'global(config.reloadOnHUP="on")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
    mode_on='config.reloadOnHUP="on"'
    mode_off='config.reloadOnHUP="off"'
fi
startup
sed "s/$mode_on/$mode_off/" "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$status" != *'result=activated active_generation=2'* ]]; then
    echo "FAIL: on-to-off publication: $status"
    error_exit 1
fi
sed "s/$mode_off/$mode_on/" "$CONF_FILE" >"$CONF_FILE.next"
mv "$CONF_FILE.next" "$CONF_FILE"
issue_HUP
status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
if [[ "$status" != *'result=ignored active_generation=2'* ]]; then
    echo "FAIL: off controller reread configuration: $status"
    error_exit 1
fi
injectmsg 0 1
shutdown_when_empty
wait_shutdown
content_check 'msgnum:00000000' "$RSYSLOG_OUT_LOG"
exit_test
