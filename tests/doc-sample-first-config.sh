#!/bin/bash
# Validate the minimal config example from
# doc/source/getting_started/beginner_tutorials/02-first-config.rst.
# The oracle is rsyslogd -C -N1 accepting the page's first modern snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock

cat > "${RSYSLOG_DYNNAME}.first-config.conf" <<'CONF_EOF'
module(load="../plugins/imuxsock/.libs/imuxsock")

if ($msg contains "error") then {
    action(type="omfile" file="/var/log/errors.log")
}
CONF_EOF

doc_sample_expect_config_valid "first-config" "${RSYSLOG_DYNNAME}.first-config.conf"
exit_test
