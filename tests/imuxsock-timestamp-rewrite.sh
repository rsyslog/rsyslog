#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Exercise the real Unix-socket receive path with malformed PRI runs, short
# timestamps at EOF, and variable-width timestamps followed by tag/body data.
# Exact raw-message checks detect unwanted rewrites; tag/body checks detect
# cursor/length disagreement. A final message from every socket and the harness
# termination marker prove processing and clean shutdown. The -vg wrapper also
# detects heap accesses that ordinary output checks cannot observe.
. "${srcdir:=.}/diag.sh" init
require_plugin imuxsock
check_command_available python3
skip_platform "SunOS" "requires Unix socket credentials and system receive timestamps"
if ! grep -q '^#define HAVE_SCM_CREDENTIALS 1$' ../config.h ||
   ! grep -q '^#define HAVE_SO_TIMESTAMP 1$' ../config.h; then
    echo 'Unix socket receive timestamps are unavailable; skipping test'
    exit 77
fi

export NUMMESSAGES=135
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
if [ "${IMUXSOCK_TEST_YAML:-no}" = yes ]; then
    require_yaml_support
    generate_conf --yaml-only
    add_yaml_conf 'modules:
  - load: "../plugins/imuxsock/.libs/imuxsock"
    sysSock.use: "off"
templates:
  - name: outfmt
    type: string
    string: "%rawmsg%|%syslogtag%|%msg%\n"
inputs:'
else
    generate_conf
    add_conf 'module(load="../plugins/imuxsock/.libs/imuxsock" sysSock.use="off")
template(name="outfmt" type="string" string="%rawmsg%|%syslogtag%|%msg%\n")'
fi

for mode in default trusted annotated nosystime chain; do
    annotate=off; trusted=off; systime=on; special=on
    case "$mode" in
        trusted) annotate=on; trusted=on ;;
        annotated) annotate=on ;;
        nosystime) systime=off ;;
        chain) special=off ;;
    esac
    if [ "${IMUXSOCK_TEST_YAML:-no}" = yes ]; then
        add_yaml_conf "  - type: imuxsock
    socket: '${RSYSLOG_DYNNAME}-${mode}.sock'
    annotate: '${annotate}'
    parseTrusted: '${trusted}'
    useSysTimeStamp: '${systime}'
    useSpecialParser: '${special}'
    ruleset: main"
    else
        add_conf "input(type=\"imuxsock\" socket=\"${RSYSLOG_DYNNAME}-${mode}.sock\"
            annotate=\"${annotate}\" parseTrusted=\"${trusted}\"
            useSysTimeStamp=\"${systime}\" useSpecialParser=\"${special}\")"
    fi
done
if [ "${IMUXSOCK_TEST_YAML:-no}" = yes ]; then
    add_yaml_conf "rulesets:
  - name: main
    script: |
      if \$inputname == 'imuxsock' then action(type=\"omfile\" file=\"${RSYSLOG_OUT_LOG}\" template=\"outfmt\")"
else
    add_conf 'if $inputname == "imuxsock" then action(type="omfile" file="'"$RSYSLOG_OUT_LOG"'" template="outfmt")'
fi
startup
python3 "$srcdir/imuxsock-timestamp-rewrite.py" send "$RSYSLOG_DYNNAME" || error_exit 1
shutdown_when_empty
wait_shutdown
python3 "$srcdir/imuxsock-timestamp-rewrite.py" check "$RSYSLOG_DYNNAME" "$RSYSLOG_OUT_LOG" || error_exit 1
exit_test
