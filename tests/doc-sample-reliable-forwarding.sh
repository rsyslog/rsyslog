#!/bin/bash
# Validate the reliable forwarding tutorial examples from
# doc/source/tutorials/reliable_forwarding.rst.
# The oracle is rsyslogd -C -N1 accepting both disk-assisted forwarding
# examples so the queueing tutorial remains aligned with current syntax. The
# testbench rewrites the workDirectory to an absolute per-test spool dir so
# validation does not depend on creating /rsyslog/work on the host.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock

mkdir -p "${RSYSLOG_DYNNAME}.spool"

cat > "${RSYSLOG_DYNNAME}.reliable-forwarding.conf" <<CONF_EOF
module(load="../plugins/imuxsock/.libs/imuxsock")
global(workDirectory="${PWD}/${RSYSLOG_DYNNAME}.spool")

action(
    type="omfwd"
    target="loghost.example.net"
    port="10514"
    protocol="tcp"
    action.resumeRetryCount="-1"
    queue.type="LinkedList"
    queue.filename="srvrfwd"
    queue.saveOnShutdown="on"
)

action(
    type="omfwd"
    target="west-loghost.example.net"
    port="10514"
    protocol="tcp"
    action.resumeRetryCount="-1"
    queue.type="LinkedList"
    queue.filename="srvrfwd1"
    queue.saveOnShutdown="on"
)

action(
    type="omfwd"
    target="east-loghost.example.net"
    protocol="tcp"
    action.resumeRetryCount="-1"
    queue.type="LinkedList"
    queue.filename="srvrfwd2"
    queue.saveOnShutdown="on"
)
CONF_EOF

doc_sample_expect_config_valid \
	"reliable-forwarding" \
	"${RSYSLOG_DYNNAME}.reliable-forwarding.conf"

exit_test
