#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Invalid startup modes are configuration errors (-N1 rejects them), but a
# normal daemon falls back to OFF and continues processing across HUP. The
# process-level -N1 status is intentional; runtime diagnostics and messages
# use omfile after synchronized shutdown. Late selection must warn exactly
# once at startup and report incomplete on every HUP without losing traffic.
. ${srcdir:=.}/diag.sh init
for mode in on unsupported; do
    if [ "${RELOAD_YAML:-0}" = 1 ]; then
        generate_conf --yaml-only
        conf="${TESTCONF_NM}.yaml"
        awk -v mode="$mode" '{ print }
             /^global:/ { print "  config.reloadOnHUP: \"" mode "\""; print "  processInternalMessages: \"on\""; print "  internalmsg.severity: \"info\"" }' "$conf" > "$RSYSLOG_DYNNAME.rewrite" || error_exit 1
        mv "$RSYSLOG_DYNNAME.rewrite" "$conf" || error_exit 1
        add_yaml_conf 'rulesets:
  - name: checkpoint_main
    script: |
      action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
        printf 'call checkpoint_main\n' > "$RSYSLOG_DYNNAME.default.conf"
        add_yaml_conf 'include:
  - path: "'$RSYSLOG_DYNNAME'.default.conf"'
    else
        generate_conf
        conf="${TESTCONF_NM}.conf"
        cp "${TESTCONF_NM}.conf" "$RSYSLOG_DYNNAME.preamble"
        printf 'global(config.reloadOnHUP="%s" processInternalMessages="on" internalmsg.severity="info")\n' "$mode" > "${TESTCONF_NM}.conf"
        cat "$RSYSLOG_DYNNAME.preamble" >> "${TESTCONF_NM}.conf"
        add_conf 'action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
    fi
    ../tools/rsyslogd -N1 -f"$conf" -M"$RSYSLOG_MODDIR" >"$RSYSLOG_DYNNAME.$mode.parser" 2>&1
    status=$?
    [ "$status" = 1 ] || error_exit 1
    startup
    wait_rsyslog_instance_pid
    injectmsg 0 1
    issue_HUP
    injectmsg 1 1
    shutdown_when_empty
    wait_shutdown
    content_check 'msgnum:00000000'
    content_check 'msgnum:00000001'
    content_check 'using off mode'
    if grep -q 'reload_validate result=' "$RSYSLOG_OUT_LOG"; then
        echo 'OFF fallback performed HUP analysis'
        error_exit 1
    fi
done
# Late enable is specific to the statement-ordered RainerScript frontend; YAML
# always places its global object first.
if [ "${RELOAD_YAML:-0}" = 1 ]; then
    exit_test
fi
generate_conf
add_conf 'global(config.reloadOnHUP="validate" processInternalMessages="on" internalmsg.severity="info")
action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
startup
wait_rsyslog_instance_pid
issue_HUP
wait_content 'cause=active_source_incomplete'
issue_HUP
injectmsg 0 1
shutdown_when_empty
wait_shutdown
content_check 'reload_validate result=incomplete'
content_check 'cause=active_source_incomplete'
[ "$(grep -c 'result=incomplete' "$RSYSLOG_OUT_LOG")" = 2 ] || error_exit 1
content_check 'msgnum:00000000'
content_check 'was seen too late; HUP analysis will report incomplete'
[ "$(grep -c 'was seen too late; HUP analysis will report incomplete' "$RSYSLOG_OUT_LOG")" = 1 ] || error_exit 1
exit_test
