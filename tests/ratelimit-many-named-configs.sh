#!/bin/bash
## Regression test for the named ratelimit configuration registry.
##
## The test creates many output-scope ratelimit configs and actions that look
## them up by name. The oracle is config-validation success: every generated
## name must be inserted into the registry and later found by action setup.
. ${srcdir:=.}/diag.sh init

if [ -z "$RSYSLOGD" ]; then
    export RSYSLOGD=../tools/rsyslogd
fi

POLICY_FILE="$(pwd)/${RSYSLOG_DYNNAME}.many-output.yaml"
trap 'rm -f "$POLICY_FILE"' EXIT
cat > "$POLICY_FILE" << 'YAMLEOF'
scope: output
mode: drop
interval: 60
burst: 5
YAMLEOF

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
'

for idx in $(seq 0 63); do
    add_conf 'ratelimit(name="many_out_'$idx'" policy="'$POLICY_FILE'")
'
done

for idx in $(seq 0 63); do
    add_conf ':msg, contains, "msgnum:" action(type="omfile" file="'$RSYSLOG_OUT_LOG'"
                                 template="outfmt"
                                 action.ratelimit.name="many_out_'$idx'")
'
done

export CONF_FILE="${TESTCONF_NM}.conf"
if ! "$RSYSLOGD" -N1 -M"$RSYSLOG_MODDIR" -f "$CONF_FILE" > stdout.log 2> stderr.log; then
    echo "FAIL: generated named ratelimit config did not validate"
    cat stdout.log
    cat stderr.log
    error_exit 1
fi

exit_test
