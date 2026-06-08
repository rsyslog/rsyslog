#!/bin/bash
# Validate the modern extension examples from
# doc/source/getting_started/understanding_default_config.rst.
# The oracle is rsyslogd -C -N1 accepting the mixed-syntax coexistence example.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock
require_plugin imjournal

cat > "${RSYSLOG_DYNNAME}.understanding-default-config.conf" <<'CONF_EOF'
module(load="../plugins/imuxsock/.libs/imuxsock")
module(load="../plugins/imjournal/.libs/imjournal")

if ($syslogfacility-text == "local3") then {
    action(type="omfile" file="/var/log/local3.log")
}
CONF_EOF

doc_sample_expect_config_valid "understanding-default-config" "${RSYSLOG_DYNNAME}.understanding-default-config.conf"
exit_test
