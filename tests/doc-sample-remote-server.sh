#!/bin/bash
# Validate the remote reception and forwarding examples from
# doc/source/getting_started/beginner_tutorials/06-remote-server.rst.
# The oracle is rsyslogd -C -N1 accepting both the UDP listener and forwarder.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imudp

cat > "${RSYSLOG_DYNNAME}.remote-server.conf" <<'CONF_EOF'
module(load="../plugins/imudp/.libs/imudp")

input(type="imudp" port="514")

action(
    type="omfwd"
    target="192.0.2.10"
    port="514"
    protocol="udp"
)
CONF_EOF

doc_sample_expect_config_valid "remote-server" "${RSYSLOG_DYNNAME}.remote-server.conf"
exit_test
