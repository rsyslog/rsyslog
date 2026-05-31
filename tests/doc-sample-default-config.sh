#!/bin/bash
# Validate the mixed-syntax extension example from
# doc/source/getting_started/beginner_tutorials/03-default-config.rst.
# The oracle is rsyslogd -C -N1 accepting the modern snippet additions.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock
require_plugin imjournal

cat > "${RSYSLOG_DYNNAME}.default-config.conf" <<'CONF_EOF'
module(load="../plugins/imuxsock/.libs/imuxsock")
module(load="../plugins/imjournal/.libs/imjournal")

if ($syslogfacility-text == "local0") then {
    action(type="omfile" file="/var/log/local0.log")
}
CONF_EOF

doc_sample_expect_config_valid "default-config" "${RSYSLOG_DYNNAME}.default-config.conf"
exit_test
