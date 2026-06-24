#!/bin/bash
# Validate the pipeline examples from
# doc/source/getting_started/beginner_tutorials/04-message-pipeline.rst.
# The oracle is rsyslogd -C -N1 accepting the filter and forwarding actions.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock

cat > "${RSYSLOG_DYNNAME}.message-pipeline.conf" <<'CONF_EOF'
module(load="../plugins/imuxsock/.libs/imuxsock")

if ($msg contains "failed") then {
    action(type="omfile" file="/var/log/failed.log")
}

if ($fromhost-ip == "10.0.0.5") then {
    action(
        type="omfwd"
        target="192.0.2.10"
        port="514"
        protocol="udp"
    )
}
CONF_EOF

doc_sample_expect_config_valid "message-pipeline" "${RSYSLOG_DYNNAME}.message-pipeline.conf"
exit_test
