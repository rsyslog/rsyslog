#!/bin/bash
# test for imtcp per-source rate limiting HUP reload
. ${srcdir:=.}/diag.sh init

# Create initial policy: limit 10
cat <<EOF > $RSYSLOG_DYNNAME.ratelimit.yaml
default:
  max: 10
  window: 60
EOF

# Rsyslog configuration
generate_conf
add_conf '
module(load="../plugins/imtcp/.libs/imtcp")
template(name="keytpl" type="string" string="%rawmsg:F,58:2%")
template(name="outfmt" type="string" string="%msg%\n")

input(type="imtcp" port="0" 
      listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port"
      perSourceRate="on"
      perSourceKeyTpl="keytpl"
      perSourcePolicyReloadOnHUP="on"
      perSourcePolicyFile="'$(pwd)/$RSYSLOG_DYNNAME.ratelimit.yaml'")

if $msg contains "rsyslogd" then stop
action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
'

startup
# send 20 messages with 'default' key
tcpflood -m 20 -P "source:default:msgnum:"
# wait for 10 messages (limited)
wait_file_lines $RSYSLOG_OUT_LOG 10
# clean log
cat /dev/null > $RSYSLOG_OUT_LOG

# update policy: add override for 'new-key' with limit 5
cat <<EOF > $RSYSLOG_DYNNAME.ratelimit.yaml
default:
  max: 10
  window: 60
overrides:
  - key: "new-key"
    max: 5
    window: 60
EOF

# HUP it
kill -HUP $(cat $RSYSLOG_DYNNAME.pid)
# give it a moment
sleep 2

# send 20 messages with 'new-key'
tcpflood -m 20 -P "source:new-key:msgnum:"
# expect 5 messages
wait_file_lines $RSYSLOG_OUT_LOG 5

shutdown_when_empty
wait_shutdown
exit_test
