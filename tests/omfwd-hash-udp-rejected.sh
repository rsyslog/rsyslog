#!/bin/bash
# pool.policy="hash" must be rejected for UDP. UDPSend() always transmits
# through target[0], so a hash policy on UDP would load and then silently
# not shard. The default protocol is UDP, so an omitted protocol is the
# same misconfiguration.
#
# Oracle: rsyslogd -N1 exits non-zero and the log contains the exact
# diagnostic below. No listener and no wait: this is config validation
# only. port="1514" is never bound. A third -N1 with protocol="tcp"
# must succeed, proving the check is the UDP restriction and not a
# blanket hash rejection.
. ${srcdir:=.}/diag.sh init

reject_udp() {
    local label="$1"
    local proto_line="$2"
    generate_conf
    add_conf '
template(name="shardkey" type="string" string="constant-shard-key")

action(type="omfwd"
       target="127.0.0.1"
       port="1514"
       '"$proto_line"'
       pool.policy="hash"
       pool.hashkey="shardkey")
'
    ../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.${label}.log" 2>&1
    if [ $? -eq 0 ]; then
        echo "FAIL: expected config validation failure for hash policy (${label})"
        cat "${RSYSLOG_DYNNAME}.${label}.log"
        error_exit 1
    fi
    content_check 'omfwd: pool.policy="hash" requires protocol="tcp"' \
        "${RSYSLOG_DYNNAME}.${label}.log"
}

reject_udp explicit 'protocol="udp"'
reject_udp default ''

generate_conf
add_conf '
template(name="shardkey" type="string" string="constant-shard-key")

action(type="omfwd"
       target="127.0.0.1"
       port="1514"
       protocol="tcp"
       pool.policy="hash"
       pool.hashkey="shardkey")
'
../tools/rsyslogd -N1 -f"${TESTCONF_NM}.conf" -M"$RSYSLOG_MODDIR" >"${RSYSLOG_DYNNAME}.tcp.log" 2>&1
if [ $? -ne 0 ]; then
    echo "FAIL: protocol=tcp with pool.policy=hash was rejected"
    cat "${RSYSLOG_DYNNAME}.tcp.log"
    error_exit 1
fi

exit_test
