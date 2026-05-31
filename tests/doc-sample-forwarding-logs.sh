#!/bin/bash
# Validate the minimal forwarding snippets from
# doc/source/getting_started/forwarding_logs.rst.
# The oracle is rsyslogd -C -N1 accepting the TCP and UDP forwarding examples
# as published, confirming the getting-started guidance stays syntactically valid.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.forwarding-logs.conf" <<CONF_EOF
action(
    type="omfwd"
    protocol="tcp"
    target="logs.example.com"
    port="514"
    queue.type="linkedList"
)

action(
    type="omfwd"
    protocol="udp"
    target="logs.example.com"
    port="514"
)
CONF_EOF

doc_sample_expect_config_valid \
	"forwarding-logs" \
	"${RSYSLOG_DYNNAME}.forwarding-logs.conf"

exit_test
