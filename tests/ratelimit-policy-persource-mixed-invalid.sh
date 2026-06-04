#!/bin/bash
## Regression test for ambiguous per-source ratelimit configuration.
## A policy YAML file that contains a perSource section is the canonical
## source of per-source settings; mixing it with legacy perSourcePolicy or
## perSourceKeyTpl on the same ratelimit object must fail at config load.

. ${srcdir:=.}/diag.sh init
if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.policy.yaml"
STDERR_FILE="${RSYSLOG_DYNNAME}.stderr.log"
export POLICY_FILE

cat > "$POLICY_FILE" <<EOF
perSource:
  enabled: true
  keyTemplate: "PerSourceKey"
  default:
    max: 5
    window: 2s
EOF

generate_conf
add_conf '
template(name="PerSourceKey" type="string" string="%hostname%")
ratelimit(name="bad"
          policy="'$POLICY_FILE'"
          perSourcePolicy="'$POLICY_FILE'")

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" ratelimit.name="bad")
'
export CONF_FILE="${TESTCONF_NM}.conf"
"$RSYSLOGD" -N1 -M"$RSYSLOG_MODDIR" -f "$CONF_FILE" > /dev/null 2> "$STDERR_FILE"

if grep -q "do not mix it with legacy perSource/perSourcePolicy/perSourceKeyTpl/maxStates/topN parameters" "$STDERR_FILE"; then
    exit_test
fi

echo "FAIL: mixed canonical and legacy per-source ratelimit config was not rejected as expected"
cat "$STDERR_FILE"
error_exit 1
