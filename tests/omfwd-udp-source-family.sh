#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# An explicit IPv4 UDP source must use a compatible DNS target even when IPv6
# comes first. The preload fixes target DNS ordering; numeric source lookups
# use libc. The optional dual-local case also controls source DNS, observes real
# binds, and checks each socket family. Run IPv4-first delivery and IPv6-
# only suspension controls before the mixed-family regression. Exact received
# payload and source address prove delivery in both sendToAll modes. A suspended-action fallback
# proves the negative control was attempted. Bounded predicate waits protect
# against hangs; no error-log silence or external DNS behavior is asserted.
. ${srcdir:=.}/diag.sh init
case $(uname -s) in Linux) ;; *) echo 'SKIP ELF resolver preload requires Linux'; exit 77 ;; esac
skip_ASAN "LD_PRELOAD conflicts with ASan runtime load order"
require_plugin imudp
if [ "${OMFWD_FAMILY_DUAL_SOURCE:-0}" = 1 ]; then
    if ! python3 -c 'import socket; s=socket.socket(socket.AF_INET6,socket.SOCK_DGRAM); s.bind(("::1",0)); s.close()'; then
        echo 'SKIP dual-source case requires an active IPv6 loopback stack'
        exit 77
    fi
fi
if [ "${OMFWD_FAMILY_YAML:-0}" = 1 ]; then require_yaml_support; fi

export RSYSLOG_UDP_FAMILY_TARGET="${RSYSLOG_DYNNAME}.invalid"
portfile="${RSYSLOG_DYNNAME}.udp.port"
fallback="${RSYSLOG_DYNNAME}.fallback"
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$portfile'")
template(name="received" type="string" string="%fromhost-ip% %msg%\n")
if $programname == "familytest" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="received")
'
startup
receiver_port=""
assign_file_content receiver_port "$portfile"

run_sender() {
    local order="$1"
    local send_to_all="${2:-off}"
    local source_address=127.0.0.1
    if [ "$order" = v4-only ]; then
        export RSYSLOG_UDP_FAMILY_SOURCE="${RSYSLOG_DYNNAME}.local.invalid"
        export RSYSLOG_UDP_FAMILY_BIND_MARKER="${RSYSLOG_DYNNAME}.bound-families"
        source_address="$RSYSLOG_UDP_FAMILY_SOURCE"
    fi
    export RSYSLOG_UDP_FAMILY_ORDER="$order"
    export RSYSLOG_PRELOAD=".libs/liboverride_omfwd_udp_family.so"
    if [ "${OMFWD_FAMILY_YAML:-0}" = 1 ]; then
        generate_conf 2
        add_conf 'include(file="'${RSYSLOG_DYNNAME}'.sender.yaml")
call main' 2
        printf '%s\n' '
templates:
  - name: wire
    type: string
    string: "<13>1 2000-01-01T00:00:00Z host familytest - - - '$order'\n"
  - name: failed
    type: string
    string: "suspended '$order'\n"
rulesets:
  - name: main
    script: |
      if $msg contains "msgnum:" then {
        action(type="omfwd" protocol="udp" target="'$RSYSLOG_UDP_FAMILY_TARGET'" port="'$receiver_port'" Address="'$source_address'" template="wire" udp.sendToAll="'$send_to_all'" action.resumeRetryCount="0")
        action(type="omfile" file="'$fallback'" template="failed" action.execOnlyWhenPreviousIsSuspended="on")
      }
' > "${RSYSLOG_DYNNAME}.sender.yaml"
    else
        generate_conf 2
        add_conf '
template(name="wire" type="string" string="<13>1 2000-01-01T00:00:00Z host familytest - - - '$order'\n")
template(name="failed" type="string" string="suspended '$order'\n")
if $msg contains "msgnum:" then {
  action(type="omfwd" protocol="udp" target="'$RSYSLOG_UDP_FAMILY_TARGET'" port="'$receiver_port'" Address="'$source_address'" template="wire" udp.sendToAll="'$send_to_all'" action.resumeRetryCount="0")
  action(type="omfile" file="'$fallback'" template="failed" action.execOnlyWhenPreviousIsSuspended="on")
}
' 2
    fi
    startup 2
    unset RSYSLOG_PRELOAD
    injectmsg2 0 1
}

run_sender v4-first
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty 2
wait_shutdown 2
content_check '127.0.0.1 v4-first' "$RSYSLOG_OUT_LOG"

run_sender v6-only
wait_file_lines --abort-on-oversize "$fallback" 1
shutdown_when_empty 2
wait_shutdown 2
content_check 'suspended v6-only' "$fallback"

final_order=v6-first
if [ "${OMFWD_FAMILY_DUAL_SOURCE:-0}" = 1 ]; then final_order=v4-only; fi
run_sender "$final_order" "${OMFWD_FAMILY_SEND_TO_ALL:-off}"
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 2
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown
printf '127.0.0.1 v4-first\n127.0.0.1 %s\n' "$final_order" > "${RSYSLOG_DYNNAME}.expected"
cmp_exact_file "${RSYSLOG_DYNNAME}.expected" "$RSYSLOG_OUT_LOG"
if [ "${OMFWD_FAMILY_DUAL_SOURCE:-0}" = 1 ]; then
    printf 'AF_INET6\nAF_INET\n' > "${RSYSLOG_DYNNAME}.expected-families"
    cmp_exact_file "${RSYSLOG_DYNNAME}.expected-families" "$RSYSLOG_UDP_FAMILY_BIND_MARKER"
fi
exit_test
