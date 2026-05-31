#!/bin/bash
# Validate the runnable RainerScript example from
# doc/source/getting_started/basic_configuration.rst.
# The oracle is rsyslogd -C -N1 accepting the copied minimal local logging
# config after the testbench rewrites module load paths for the build tree.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imuxsock
require_plugin imklog

cat > "${RSYSLOG_DYNNAME}.basic-configuration.conf" <<CONF_EOF
module(load="../plugins/imuxsock/.libs/imuxsock")
module(load="../plugins/imklog/.libs/imklog")
action(type="omfile" file="${RSYSLOG_OUT_LOG}")
CONF_EOF

doc_sample_expect_config_valid \
	"basic-configuration" \
	"${RSYSLOG_DYNNAME}.basic-configuration.conf"

exit_test
