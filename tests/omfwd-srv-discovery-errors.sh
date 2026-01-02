#!/bin/bash
## @brief Validate SRV error handling and mutual exclusivity for omfwd targetSrv.
. ${srcdir:=.}/diag.sh init
check_command_available python3

DNS_RECORDS="$RSYSLOG_DYNNAME.srv.json"
DNS_PORT_FILE="$RSYSLOG_DYNNAME.dns_port"

cat >"$DNS_RECORDS" <<EOFJSON
{
  "SRV": {},
  "A": {}
}
EOFJSON

python3 "$srcdir/dns_srv_mock.py" --port 0 --port-file "$DNS_PORT_FILE" --records "$DNS_RECORDS" &
DNS_PID=$!
wait_file_exists "$DNS_PORT_FILE"
DNS_PORT=$(cat "$DNS_PORT_FILE")
export RSYSLOG_DNS_SERVER=127.0.0.1
export RSYSLOG_DNS_PORT="$DNS_PORT"
export RES_OPTIONS="attempts:1 timeout:1"
trap 'kill $DNS_PID 2>/dev/null; rm -f "$DNS_PORT_FILE" "$DNS_RECORDS"' EXIT

# missing SRV records should fail config check
generate_conf
add_conf '
module(load="builtin:omfwd")
action(type="omfwd" targetSrv="nosrv.test" protocol="tcp")
'

if rsyslogd_config_check; then
    echo "Expected configuration failure when SRV lookup has no answers"
    exit 1
fi

# conflicting parameters should also fail
generate_conf
add_conf '
module(load="builtin:omfwd")
action(type="omfwd" targetSrv="nosrv.test" target="127.0.0.1" protocol="tcp")
'

if rsyslogd_config_check; then
    echo "Expected configuration failure when target and targetSrv are combined"
    exit 1
fi

kill $DNS_PID 2>/dev/null
rm -f "$DNS_PORT_FILE" "$DNS_RECORDS"
unset RSYSLOG_DNS_SERVER RSYSLOG_DNS_PORT
unset RES_OPTIONS
trap - EXIT
exit_test
