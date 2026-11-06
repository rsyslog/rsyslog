#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Private HUP parsing never activates actions; rejected mode changes, legacy
# directives, strict missing includes and malformed candidates followed
# by a valid one prove recovery. Rotation on both success and failure proves
# legacy omfile reopen. Port files, completed-diagnostic counts and message
# content synchronize phases; normal diagnostics use omfile, not stderr.
# A runtime function expression must preserve the common AST tag in OFF and
# validate mode, while private capture/destruction must not bind callbacks.
# A self-referential optional include proves non-missing I/O errors reject even
# as root. Unprivileged runs also retain the permission-denied directory case;
# root bypasses mode bits, so that specific permission oracle is inapplicable.
# A candidate with invalid runtime semantics is structurally compared while
# the separate -N1 process rejects it; its process diagnostics are the explicit
# config-check oracle, never a substitute for the daemon omfile HUP reports.
# Stream edits use POSIX sed/awk syntax on both GNU and BSD implementations.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp
rewrite_conf() {
    sed "$1" "$conf" > "$RSYSLOG_DYNNAME.rewrite" || error_exit 1
    mv "$RSYSLOG_DYNNAME.rewrite" "$conf" || error_exit 1
}
if [ "${RELOAD_YAML:-0}" = 1 ]; then
    generate_conf --yaml-only
    conf="${TESTCONF_NM}.yaml"
    awk '{ print }
         /^global:/ {
             print "  config.reloadOnHUP: \"validate\""
             print "  processInternalMessages: \"on\""
             print "  internalmsg.severity: \"info\""
         }' "$conf" > "$RSYSLOG_DYNNAME.rewrite" || error_exit 1
    mv "$RSYSLOG_DYNNAME.rewrite" "$conf" || error_exit 1
    add_yaml_conf 'modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    address: "127.0.0.1"
    port: "0"
    listenPortFileName: "'$RSYSLOG_DYNNAME'.tcpflood_port"
rulesets:
  - name: checkpoint_main
    script: |
      set $.checkpoint = strlen("checkpoint");
      if $msg contains "reload_validate" then action(type="omfile" file="'$RSYSLOG_DYNNAME'.diagnostics")
      if $msg contains "reload-marker" and $.checkpoint == 10 then action(name="retained" type="omfile" file="'$RSYSLOG_OUT_LOG'" flushOnTXEnd="on")'
else
    generate_conf
    conf="${TESTCONF_NM}.conf"
    cp "$conf" "$RSYSLOG_DYNNAME.preamble"
    printf '%s\n' 'global(config.reloadOnHUP="validate" processInternalMessages="on" internalmsg.severity="info")' > "$conf"
    cat "$RSYSLOG_DYNNAME.preamble" >> "$conf"
    add_conf 'module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" address="127.0.0.1" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")
set $.checkpoint = strlen("checkpoint");
if $msg contains "reload_validate" then action(type="omfile" file="'$RSYSLOG_DYNNAME'.diagnostics")
if $msg contains "reload-marker" and $.checkpoint == 10 then action(name="retained" type="omfile" file="'$RSYSLOG_OUT_LOG'" flushOnTXEnd="on")'
fi
# Relative includes must resolve from the startup configuration context even
# after daemon startup changes cwd. Disabled actions must remain absent from
# both structural snapshots, so an unchanged candidate has zero differences.
printf 'template(name="relativeInclude" type="string" string="%%msg%%")\n' > "$RSYSLOG_DYNNAME.relative.conf"
if [ "${RELOAD_YAML:-0}" = 1 ]; then
    printf 'call checkpoint_main\n' >> "$RSYSLOG_DYNNAME.relative.conf"
    printf '      action(config.enabled="off" type="omfile" file="%s.disabled")\n' "$RSYSLOG_DYNNAME" >> "$conf"
    printf '\ninclude:\n  - path: "%s.relative.conf"\n' "$RSYSLOG_DYNNAME" >> "$conf"
else
    printf '\naction(config.enabled="off" type="omfile" file="%s.disabled")\ninclude(file="%s.relative.conf")\n' "$RSYSLOG_DYNNAME" "$RSYSLOG_DYNNAME" >> "$conf"
fi
cp "$conf" "$RSYSLOG_DYNNAME.active"
startup
send_marker() {
    tcpflood -m1 -M"<167>Mar  1 01:00:00 localhost test: reload-marker-$1"
    wait_content "reload-marker-$1" "$RSYSLOG_OUT_LOG"
}
# Count completed parses so identical rejected candidates cannot reuse a stale
# diagnostic. The deadline only bounds a failed oracle under loaded CI.
wait_result_count() {
    local deadline=$((SECONDS + TB_TEST_TIMEOUT))
    while [ "$(grep -c "$1" "$RSYSLOG_DYNNAME.diagnostics" 2>/dev/null || true)" != "$2" ]; do
        [ "$SECONDS" -lt "$deadline" ] || error_exit 1
        "$TESTTOOL_DIR/msleep" 100
    done
}
send_marker before
issue_HUP
wait_result_count 'result=structurally_compared' 1
wait_content 'added=0 removed=0 modified=0' "$RSYSLOG_DYNNAME.diagnostics"
sed "s/${RSYSLOG_OUT_LOG}/${RSYSLOG_DYNNAME}.candidate/g" "$RSYSLOG_DYNNAME.active" > "$conf"
mv "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.rotated-success"
issue_HUP
wait_result_count 'result=structurally_compared' 2
send_marker success
attempt=0
for variant in malformed malformed off on removed-mode legacy missing missing optional-loop optional-io bad-disabled; do
    if [ "$variant" = optional-io ] && [ "$(id -u)" -eq 0 ]; then
        continue
    fi
    attempt=$((attempt + 1))
    cp "$RSYSLOG_DYNNAME.active" "$conf"
    case "$variant" in
    off|on)
        rewrite_conf "s/\"validate\"/\"$variant\"/g"
        ;;
    removed-mode)
        # Drop just the startup mode parameter, preserving all other objects.
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            rewrite_conf '/config.reloadOnHUP:/d'
        else
            rewrite_conf 's/config.reloadOnHUP="validate" //'
        fi
        ;;
    legacy)
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            awk '/^include:/ { print "      $ResetConfigVariables" }
                 { print }' "$conf" > "$RSYSLOG_DYNNAME.rewrite" || error_exit 1
            mv "$RSYSLOG_DYNNAME.rewrite" "$conf" || error_exit 1
        else
            printf '$ResetConfigVariables\n' >> "$conf"
        fi
        ;;
    missing)
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            printf '  - path: "%s.missing"\n' "$RSYSLOG_DYNNAME" >> "$conf"
        else
            printf '\ninclude(file="%s.missing" mode="abort-if-missing")\n' "$RSYSLOG_DYNNAME" >> "$conf"
        fi
        ;;
    bad-disabled)
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            rewrite_conf 's/config.enabled="off"/config.enabled=["off"]/'
        else
            printf '\naction(config.enabled=["off"] type="omfile" file="%s.bad-disabled")\n' "$RSYSLOG_DYNNAME" >> "$conf"
        fi
        ;;
    optional-loop)
        ln -s "$RSYSLOG_DYNNAME.include-loop" "$RSYSLOG_DYNNAME.include-loop" || error_exit 1
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            printf '  - path: "%s.include-loop"\n    optional: "on"\n' "$RSYSLOG_DYNNAME" >> "$conf"
        else
            printf '\ninclude(file="%s.include-loop" mode="optional")\n' "$RSYSLOG_DYNNAME" >> "$conf"
        fi
        ;;
    optional-io)
        mkdir "$RSYSLOG_DYNNAME.unreadable"
        chmod 000 "$RSYSLOG_DYNNAME.unreadable"
        if [ "${RELOAD_YAML:-0}" = 1 ]; then
            printf '  - path: "%s.unreadable/*.conf"\n    optional: "on"\n' "$RSYSLOG_DYNNAME" >> "$conf"
        else
            printf '\ninclude(file="%s.unreadable/*.conf" mode="optional")\n' "$RSYSLOG_DYNNAME" >> "$conf"
        fi
        ;;
    malformed)
        printf '\naction(type="omfile" file=\n' >> "$conf"
        ;;
    esac
    if [ "$attempt" = 1 ]; then
        mv "$RSYSLOG_OUT_LOG" "$RSYSLOG_DYNNAME.rotated-failure"
    fi
    issue_HUP
    wait_result_count 'result=rejected' "$attempt"
    if [ "$variant" = optional-loop ]; then
        rm "$RSYSLOG_DYNNAME.include-loop" || error_exit 1
    elif [ "$variant" = optional-io ]; then
        chmod 700 "$RSYSLOG_DYNNAME.unreadable"
        rmdir "$RSYSLOG_DYNNAME.unreadable"
    fi
    send_marker "failure-$attempt"
done
# These constructs parse structurally but are not a valid runtime config.
cp "$RSYSLOG_DYNNAME.active" "$conf"
if [ "${RELOAD_YAML:-0}" = 1 ]; then
    awk '/^include:/ {
             print "      action(type=\"checkpoint_unknown_action\")"
             print "      action(type=\"omfile\" file=\"'"$RSYSLOG_DYNNAME"'.invalid-params\" checkpoint_invalid_parameter=\"on\")"
             print "      call checkpoint_missing_ruleset"
         }
         { print }' "$conf" > "$RSYSLOG_DYNNAME.rewrite" || error_exit 1
    mv "$RSYSLOG_DYNNAME.rewrite" "$conf" || error_exit 1
else
    printf '\naction(type="checkpoint_unknown_action")\naction(type="omfile" file="%s.invalid-params" checkpoint_invalid_parameter="on")\ncall checkpoint_missing_ruleset\n' "$RSYSLOG_DYNNAME" >> "$conf"
fi
../tools/rsyslogd -N1 -f"$conf" -M"$RSYSLOG_MODDIR" > "$RSYSLOG_DYNNAME.semantic-check" 2>&1
[ "$?" = 1 ] || error_exit 1
content_check 'checkpoint_unknown_action' "$RSYSLOG_DYNNAME.semantic-check"
content_check 'checkpoint_invalid_parameter' "$RSYSLOG_DYNNAME.semantic-check"
content_check 'checkpoint_missing_ruleset' "$RSYSLOG_DYNNAME.semantic-check"
issue_HUP
wait_result_count 'result=structurally_compared' 3
send_marker structural-only
cp "$RSYSLOG_DYNNAME.active" "$conf"
issue_HUP
wait_result_count 'result=structurally_compared' 4
send_marker recovered
shutdown_when_empty
wait_shutdown
content_check 'reload-marker-before' "$RSYSLOG_DYNNAME.rotated-success"
content_check 'reload-marker-success' "$RSYSLOG_DYNNAME.rotated-failure"
content_check 'reload-marker-failure-1'
content_check 'reload-marker-failure-2'
content_check 'reload-marker-recovered'
content_check 'result=structurally_compared activated=0 analysis=structural_only' "$RSYSLOG_DYNNAME.diagnostics"
content_check --regex 'added=[0-9].*removed=[0-9].*modified=[0-9].*unchanged=[0-9]' "$RSYSLOG_DYNNAME.diagnostics"
content_check 'change=modified instance=' "$RSYSLOG_DYNNAME.diagnostics"
content_check 'kind=action' "$RSYSLOG_DYNNAME.diagnostics"
content_check 'kind=ruleset' "$RSYSLOG_DYNNAME.diagnostics"
content_check 'reason=activation_not_implemented' "$RSYSLOG_DYNNAME.diagnostics"
content_check 'cause=private_parse_failed' "$RSYSLOG_DYNNAME.diagnostics"
[ ! -e "$RSYSLOG_DYNNAME.candidate" ] || error_exit 1
[ ! -e "$RSYSLOG_DYNNAME.invalid-params" ] || error_exit 1
[ ! -e "$RSYSLOG_DYNNAME.disabled" ] || error_exit 1
exit_test
