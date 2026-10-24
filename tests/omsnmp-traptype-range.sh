#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Decode real loopback SNMPv1 datagrams: configured 0/3/6 remain unchanged,
# while out-of-range -1 and 7 and the omitted value use 6. The receiver
# publishes its port only after binding; a bounded receive timeout detects missing sends. Its exit
# status and exact decoded fields are the oracle, followed by synchronized
# rsyslog shutdown. The negative-case -N1 check permits only its expected fallback error; all
# other parsing errors fail before traffic. No external SNMP service is needed.
. ${srcdir:=.}/diag.sh init
require_plugin omsnmp
receiver_pid=
cleanup_receiver() {
    if [ -n "$receiver_pid" ]; then
        kill "$receiver_pid" 2>/dev/null || true
        wait "$receiver_pid" 2>/dev/null || true
    fi
}
trap cleanup_receiver EXIT
$PYTHON "$srcdir/omsnmp-traptype-receiver.py" "$RSYSLOG_DYNNAME.port" "$RSYSLOG_OUT_LOG" 6 &
receiver_pid=$!
wait_file_exists "$RSYSLOG_DYNNAME.port"
port=$(cat "$RSYSLOG_DYNNAME.port")
generate_conf
if [ "${OMSNMP_TEST_YAML:-0}" = 1 ]; then
    require_yaml_support
    add_conf '
include(file="'"$RSYSLOG_DYNNAME"'.yaml")
call main
'
    cat > "$RSYSLOG_DYNNAME.yaml" <<YAML_EOF
modules:
  - load: "../plugins/omsnmp/.libs/omsnmp"
rulesets:
  - name: main
    filter: ':msg, contains, "msgnum:"'
    actions:
YAML_EOF
else
    add_conf '
module(load="../plugins/omsnmp/.libs/omsnmp")
'
fi
for traptype in 0 3 6 7 -1 omitted; do
    specific=$traptype
    [ "$traptype" = -1 ] && specific=99
    setting='trapType="'"$traptype"'"'
    yaml_setting='        trapType: "'"$traptype"'"'
    if [ "$traptype" = omitted ]; then
        specific=98
        setting=
        yaml_setting=
    fi
    if [ "${OMSNMP_TEST_YAML:-0}" = 1 ]; then
        cat >> "$RSYSLOG_DYNNAME.yaml" <<YAML_EOF
      - type: omsnmp
        server: "127.0.0.1"
        port: "$port"
        version: "0"
        community: public
$yaml_setting
        specificType: "$specific"
YAML_EOF
    else
        add_conf '
if $msg contains "msgnum:" then action(type="omsnmp"
            server="127.0.0.1" port="'"$port"'" version="0" community="public"
            '"$setting"' specificType="'"$specific"'")
'
    fi
    if [ "$traptype" = 6 ]; then
        rsyslogd_config_check > "$RSYSLOG_DYNNAME.valid-preflight" 2>&1 || {
            cat "$RSYSLOG_DYNNAME.valid-preflight"
            error_exit 1 "valid SNMP controls failed -N1"
        }
    fi
done
# The corrected range check reports the intentional invalid values as errors
# while retaining usable fallback actions. Never accept another config error.
if rsyslogd_config_check > "$RSYSLOG_DYNNAME.preflight" 2>&1; then
    error_exit 1 "invalid SNMP values did not produce fallback diagnostic"
fi
content_count_check "omsnmp: traptype invalid, setting to ENTERPRISESPECIFIC" 2 "$RSYSLOG_DYNNAME.preflight"
if grep -E 'error during parsing|error:|could not|unknown' "$RSYSLOG_DYNNAME.preflight" |
    grep -v 'omsnmp: traptype invalid, setting to ENTERPRISESPECIFIC'; then
    cat "$RSYSLOG_DYNNAME.preflight"
    error_exit 1 "unexpected generated-config error"
fi
cat "$RSYSLOG_DYNNAME.preflight"
startup
injectmsg 0 1
if ! wait "$receiver_pid"; then
    receiver_pid=
    error_exit 1 "SNMP receiver failed or timed out"
fi
receiver_pid=
shutdown_when_empty
wait_shutdown
printf '0:0\n3:3\n6:6\n7:6\n98:6\n99:6\n' > "$RSYSLOG_DYNNAME.expected"
cmp_exact_file "$RSYSLOG_DYNNAME.expected" "$RSYSLOG_OUT_LOG"
exit_test
