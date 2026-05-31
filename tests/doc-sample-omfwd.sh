#!/bin/bash
# Validate the fuller omfwd examples from
# doc/source/configuration/modules/omfwd.rst.
# The oracle is rsyslogd -C -N1 accepting the page's modern forwarding
# examples that can validate offline. The targetSrv example is left to the
# dedicated SRV-discovery test family because config validation performs a real
# DNS lookup and fails without an SRV-backed fixture.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.omfwd.conf" <<CONF_EOF
action(
    type="omfwd"
    target="logs.example.com"
    port="514"
    protocol="tcp"
    queue.type="linkedList"
)

action(
    type="omfwd"
    target="logs.example.com"
    port="6514"
    protocol="tcp"
    StreamDriver="ossl"
    StreamDriverMode="1"
    StreamDriverAuthMode="x509/name"
    StreamDriverPermittedPeers="logs.example.com"
)

action(
    type="omfwd"
    target="remote.example.com"
    port="514"
    protocol="tcp"
    queue.type="linkedList"
)

action(
    type="omfwd"
    target="remote.example.com"
    port="515"
    protocol="udp"
)
CONF_EOF

doc_sample_expect_config_valid \
	"omfwd" \
	"${RSYSLOG_DYNNAME}.omfwd.conf"

exit_test
