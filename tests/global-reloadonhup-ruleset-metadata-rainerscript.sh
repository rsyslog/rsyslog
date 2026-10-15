#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Ruleset programs can swap, but existing queue/parser attributes cannot. An
# input binds directly to the tested ruleset, avoiding static-call rejection.
# HUP completion plus unchanged generation rejects metadata-only and combined
# edits; post-rejection delivery proves the old body remains active. A case/
# parameter-order no-op must remain generation one. The final body-only change
# must activate generation two and filter exactly the selected record. Queue
# type itself is not inspected; rejection occurs before any plan publication.
. "${srcdir:=.}/diag.sh" init
require_plugin imtcp
if [[ "${RSYSLOG_RELOAD_METADATA_YAML:-0}" == 1 ]]; then
    require_yaml_support
    generate_conf --yaml-only
    CONF_FILE="${TESTCONF_NM}.yaml"
    awk '{print} /debug.abortOnProgramError:/ {
        print "  config.reloadOnHUP: \"on\""
        print "  processInternalMessages: \"on\""
    }' "$CONF_FILE" >"$CONF_FILE.tmp"
    mv "$CONF_FILE.tmp" "$CONF_FILE"
else
    generate_conf
    CONF_FILE="${TESTCONF_NM}.conf"
    add_conf 'global(processInternalMessages="on" config.reloadOnHUP="on")'
fi
cp "$CONF_FILE" "$CONF_FILE.preamble"
write_candidate() {
    local queue_type="$1" needle="$2" spelling="$3" parser_attribute="$4"
    cp "$CONF_FILE.preamble" "$CONF_FILE"
    if [[ "${RSYSLOG_RELOAD_METADATA_YAML:-0}" == 1 ]]; then
        cat >>"$CONF_FILE" <<EOF
modules:
  - load: "../plugins/imtcp/.libs/imtcp"
inputs:
  - type: imtcp
    port: "0"
    listenPortFileName: "$RSYSLOG_DYNNAME.tcpflood_port"
    ruleset: prepared
rulesets:
EOF
        if [[ "$spelling" == prepared ]]; then
            printf '  - name: "%s"\n    queue.type: "%s"\n' "$spelling" "$queue_type" >>"$CONF_FILE"
        else
            printf '  - QUEUE.TYPE: "%s"\n    NAME: "%s"\n' "$queue_type" "$spelling" >>"$CONF_FILE"
        fi
        if [[ "$parser_attribute" == yes ]]; then
            printf '    parser: ["rsyslog.rfc3164"]\n' >>"$CONF_FILE"
        fi
        cat >>"$CONF_FILE" <<EOF
    statements:
      - if: '\$msg contains "$needle"'
        action:
          type: omfile
          name: sink
          file: "$RSYSLOG_OUT_LOG"
EOF
    else
        cat >>"$CONF_FILE" <<EOF
action(type="omfile" file="$RSYSLOG_OUT_LOG")
module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="$RSYSLOG_DYNNAME.tcpflood_port" ruleset="prepared")
EOF
        if [[ "$spelling" == prepared ]]; then
            printf 'ruleset(name="%s" queue.type="%s"' "$spelling" "$queue_type" >>"$CONF_FILE"
        else
            printf 'ruleset(QUEUE.TYPE="%s" NAME="%s"' "$queue_type" "$spelling" >>"$CONF_FILE"
        fi
        if [[ "$parser_attribute" == yes ]]; then
            printf ' parser=["rsyslog.rfc3164"]' >>"$CONF_FILE"
        fi
        cat >>"$CONF_FILE" <<EOF
) { if \$msg contains "$needle" then action(type="omfile" name="sink" file="$RSYSLOG_OUT_LOG") }
EOF
    fi
}
check_hup() {
    issue_HUP
    local status
    status="$(echo getreloadstatus | "$TESTTOOL_DIR/diagtalker" -p"$IMDIAG_PORT")"
    if [[ "$status" != *"result=$1 active_generation=$2 "* ]]; then
        echo "FAIL: metadata/body admission mismatch: $status"
        error_exit 1
    fi
}
write_candidate LinkedList msgnum prepared no
startup
tcpflood -m1 -i0
wait_queueempty
write_candidate LinkedList msgnum PrEpArEd no
check_hup reported_only 1
tcpflood -m1 -i1
wait_queueempty
write_candidate FixedArray msgnum PrEpArEd no
check_hup candidate_scope_unsupported 1
tcpflood -m1 -i2
wait_queueempty
write_candidate FixedArray never-match PrEpArEd no
check_hup candidate_scope_unsupported 1
tcpflood -m1 -i3
wait_queueempty
write_candidate LinkedList msgnum PrEpArEd yes
check_hup candidate_scope_unsupported 1
tcpflood -m1 -i4
wait_queueempty
write_candidate LinkedList msgnum:00000009 PrEpArEd no
check_hup activated 2
tcpflood -m2 -i8
wait_queueempty
shutdown_when_empty
wait_shutdown
for record in 0 1 2 3 4 9; do
    content_check "msgnum:0000000$record"
done
check_not_present 'msgnum:00000008'
exit_test
