#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A failed explicit-source socket must suspend the action. Restrict the sender
# to IPv4, then use numeric ::1 as its source: getaddrinfo cannot resolve that
# literal for AF_INET, without DNS or an active IPv6 stack. A preceding valid
# source action must deliver exactly one control packet to a real port-0
# receiver; the failing action must activate exactly one suspended fallback.
# Bounded line predicates prove both outcomes. No preload is used, so the
# socket-failure cleanup path also runs in sanitizer builds.
. ${srcdir:=.}/diag.sh init
require_plugin imudp
if [ "${OMFWD_FAILURE_YAML:-0}" = 1 ]; then require_yaml_support; fi
portfile="${RSYSLOG_DYNNAME}.udp.port"
fallback="${RSYSLOG_DYNNAME}.fallback"
generate_conf
add_conf '
module(load="../plugins/imudp/.libs/imudp")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="'$portfile'")
template(name="received" type="string" string="%fromhost-ip% %msg%\n")
if $programname == "sourcefailure" then action(type="omfile" file="'$RSYSLOG_OUT_LOG'" template="received")
'
startup
receiver_port=""
assign_file_content receiver_port "$portfile"
export RSTB_NET_IPPROTO=ipv4-only
generate_conf 2
unset RSTB_NET_IPPROTO
if [ "${OMFWD_FAILURE_YAML:-0}" = 1 ]; then
    cat > "${RSYSLOG_DYNNAME}.sender.yaml" <<YAML_EOF
templates:
  - name: wire
    type: string
    string: "<13>1 2000-01-01T00:00:00Z host sourcefailure - - - control\\n"
  - name: failed
    type: string
    string: "source-suspended\\n"
rulesets:
  - name: forwarding
    actions:
      - type: omfwd
        protocol: udp
        target: "127.0.0.1"
        port: "$receiver_port"
        Address: "127.0.0.1"
        template: wire
      - type: omfwd
        protocol: udp
        target: "127.0.0.1"
        port: "$receiver_port"
        Address: "::1"
        template: wire
        action.resumeRetryCount: "0"
      - type: omfile
        file: "$fallback"
        template: failed
        action.execOnlyWhenPreviousIsSuspended: "on"
YAML_EOF
    add_conf 'include(file="'${RSYSLOG_DYNNAME}'.sender.yaml")
if $msg contains "msgnum:" then call forwarding' 2
else
    add_conf '
template(name="wire" type="string" string="<13>1 2000-01-01T00:00:00Z host sourcefailure - - - control\n")
template(name="failed" type="string" string="source-suspended\n")
if $msg contains "msgnum:" then {
  action(type="omfwd" protocol="udp" target="127.0.0.1" port="'$receiver_port'" Address="127.0.0.1" template="wire")
  action(type="omfwd" protocol="udp" target="127.0.0.1" port="'$receiver_port'" Address="::1" template="wire" action.resumeRetryCount="0")
  action(type="omfile" file="'$fallback'" template="failed" action.execOnlyWhenPreviousIsSuspended="on")
}
' 2
fi
startup 2
injectmsg2 0 1
wait_file_lines --abort-on-oversize "$RSYSLOG_OUT_LOG" 1
wait_file_lines --abort-on-oversize "$fallback" 1
shutdown_when_empty 2
wait_shutdown 2
shutdown_when_empty
wait_shutdown
export EXPECTED="127.0.0.1 control"
cmp_exact "$RSYSLOG_OUT_LOG"
export EXPECTED="source-suspended"
cmp_exact "$fallback"
exit_test
