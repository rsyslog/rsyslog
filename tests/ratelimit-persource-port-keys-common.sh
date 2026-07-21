#!/bin/bash
## Verify the three exact port-based per-source key modes for a TCP input.
## The first tcpflood run uses two simultaneous connections and the second
## uses 32; both deterministically send two messages per connection. A limit
## of one must therefore emit exactly one message for each of 34 distinct
## numeric peer ports. The impstats oracle also proves that none of the exact
## modes used generic template or
## message-parse evaluation, even though the syslog-header hostname is set to
## an unrelated value.
: "${PORT_KEY_MODULE:?wrapper must select imtcp or imptcp}"
. ${srcdir:=.}/diag.sh init
require_plugin impstats
export STATSFILE="$RSYSLOG_DYNNAME.stats"

for mode in port ip_port host_port; do
	policy="$(pwd)/${RSYSLOG_DYNNAME}.${mode}.yaml"
	case "$mode" in
		port) template=PortKey ;;
		ip_port) template=IpPortKey ;;
		host_port) template=HostPortKey ;;
	esac
	cat >"$policy" <<EOF
perSource:
  enabled: true
  keyTemplate: "$template"
  default:
    max: 1
    window: 1h
EOF
done

generate_conf
add_conf '
template(name="PortKey" type="string" string="%fromhost-port%")
template(name="IpPortKey" type="string" string="%fromhost-ip%:%fromhost-port%")
template(name="HostPortKey" type="string" string="%fromhost%:%fromhost-port%")
ratelimit(name="port" policy="'"$(pwd)/${RSYSLOG_DYNNAME}.port.yaml"'")
ratelimit(name="ip_port" policy="'"$(pwd)/${RSYSLOG_DYNNAME}.ip_port.yaml"'")
ratelimit(name="host_port" policy="'"$(pwd)/${RSYSLOG_DYNNAME}.host_port.yaml"'")

module(load="../plugins/'"$PORT_KEY_MODULE"'/.libs/'"$PORT_KEY_MODULE"'")
module(load="../plugins/impstats/.libs/impstats" log.file="'"$STATSFILE"'" interval="1")

ruleset(name="port_rules") {
  action(type="omfile" file="'"$RSYSLOG_DYNNAME"'.port.out"
         template="PortKey")
}
ruleset(name="ip_port_rules") {
  action(type="omfile" file="'"$RSYSLOG_DYNNAME"'.ip_port.out"
         template="IpPortKey")
}
ruleset(name="host_port_rules") {
  action(type="omfile" file="'"$RSYSLOG_DYNNAME"'.host_port.out"
         template="HostPortKey")
}

input(type="'"$PORT_KEY_MODULE"'" address="127.0.0.1" port="0"
      listenPortFileName="'"$RSYSLOG_DYNNAME"'.port.listen"
      ruleset="port_rules" ratelimit.name="port")
input(type="'"$PORT_KEY_MODULE"'" address="127.0.0.1" port="0"
      listenPortFileName="'"$RSYSLOG_DYNNAME"'.ip_port.listen"
      ruleset="ip_port_rules" ratelimit.name="ip_port")
input(type="'"$PORT_KEY_MODULE"'" address="127.0.0.1" port="0"
      listenPortFileName="'"$RSYSLOG_DYNNAME"'.host_port.listen"
      ruleset="host_port_rules" ratelimit.name="host_port")
'
startup

for mode in port ip_port host_port; do
	tcpflood -p"$(cat "${RSYSLOG_DYNNAME}.${mode}.listen")" -c2 -Y -m4 \
		-h"header-${mode}" >/dev/null
	wait_file_lines "${RSYSLOG_DYNNAME}.${mode}.out" 2 30
done
for mode in port ip_port host_port; do
	tcpflood -p"$(cat "${RSYSLOG_DYNNAME}.${mode}.listen")" -c32 -Y -m64 \
		-h"other-header-${mode}" >/dev/null
done
sleep 2
shutdown_when_empty
wait_shutdown

for mode in port ip_port host_port; do
	output="${RSYSLOG_DYNNAME}.${mode}.out"
	if [ "$(wc -l <"$output")" -ne 34 ]; then
		echo "FAIL: $PORT_KEY_MODULE $mode did not allow one message per connection"
		cat "$output"
		error_exit 1
	fi
	if [ "$(sort -u "$output" | wc -l)" -ne 34 ]; then
		echo "FAIL: $PORT_KEY_MODULE $mode did not produce 34 distinct port keys"
		cat "$output"
		error_exit 1
	fi
	if sed 's/.*://' "$output" | grep -Ev '^[0-9]+$' | grep -q .; then
		echo "FAIL: $PORT_KEY_MODULE $mode emitted a non-numeric peer port"
		cat "$output"
		error_exit 1
	fi
	stats=$(grep "ratelimit.${mode}.per_source" "$STATSFILE" | tail -1)
	tpl_evals=$(printf '%s\n' "$stats" | grep -o 'per_source_key_tpl_evals=[0-9]*' | sed 's/.*=//')
	parse_evals=$(printf '%s\n' "$stats" | grep -o 'per_source_key_parse_evals=[0-9]*' | sed 's/.*=//')
	if [ "${tpl_evals:-1}" -ne 0 ] || [ "${parse_evals:-1}" -ne 0 ]; then
		echo "FAIL: $PORT_KEY_MODULE $mode used generic key evaluation: $stats"
		error_exit 1
	fi
done

exit_test
