#!/bin/bash
# Validate the stop-order examples from
# doc/source/getting_started/beginner_tutorials/05-order-matters.rst.
# The oracle is rsyslogd -C -N1 accepting both ordered rule variants.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock

cat > "${RSYSLOG_DYNNAME}.order-matters.conf" <<'CONF_EOF'
module(load="../plugins/imuxsock/.libs/imuxsock")

if ($msg contains "error") then {
    action(type="omfile" file="/var/log/errors.log")
    stop
}

action(type="omfile" file="/var/log/all.log")

if ($msg contains "error") then {
    action(type="omfile" file="/var/log/errors.log")
}

action(type="omfile" file="/var/log/all.log")
CONF_EOF

doc_sample_expect_config_valid "order-matters" "${RSYSLOG_DYNNAME}.order-matters.conf"
exit_test
