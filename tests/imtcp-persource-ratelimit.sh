#!/bin/bash
# added 2025-12-28 by rgerhards/antigravity, released under ASL 2.0
. ${srcdir:=.}/diag.sh init

# Create the YAML policy file
cat <<EOF > $RSYSLOG_DYNNAME.ratelimit.yaml
default:
  max: 10
  window: 60
overrides:
  - key: "override-key"
    max: 20
    window: 60
EOF

generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")

template(name="keytpl" type="string" string="%rawmsg:F,58:2%")
template(name="outfmt" type="string" string="%msg%\n")

input(type="imtcp" port="0" 
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      perSourceRate="on"
      perSourceKeyTpl="keytpl"
      perSourcePolicyFile="'$(pwd)/$RSYSLOG_DYNNAME.ratelimit.yaml'")

# Filter out rsyslog internal messages to have a clean count
if $msg contains "rsyslogd" then stop
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'
startup

# Use "source:default:msgnum:" -> field 2 is "default"
echo "Starting test for default limit..."
tcpflood -m 100 -P "source:default:msgnum:"
wait_file_lines $RSYSLOG_OUT_LOG 10

# Use "source:override-key:msgnum:" -> field 2 is "override-key"
echo "Starting test for override limit..."
tcpflood -m 100 -P "source:override-key:msgnum:"
wait_file_lines $RSYSLOG_OUT_LOG 30

shutdown_when_empty
wait_shutdown

# Check total lines: 10 (default) + 20 (override) = 30
LINES=$(wc -l < $RSYSLOG_OUT_LOG)
if [ "$LINES" -ne 30 ]; then
    echo "FAILED: Expected 30 lines, got $LINES"
    exit 1
fi

echo "SUCCESS: Per-source rate limiting test passed"
exit_test
