#!/bin/bash
# Validate the RainerScript configuration blocks from
# doc/source/configuration/basic_structure.rst.
# The oracle is rsyslogd -C -N1 accepting a config assembled from the page's
# rule, input, action, and ruleset examples with only build-tree path fixes.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin imtcp

cat > "${RSYSLOG_DYNNAME}.basic-structure.conf" <<CONF_EOF
if \$syslogfacility-text == 'mail' and \$syslogseverity-text == 'err' then {
    action(type="omfile" file="${RSYSLOG_OUT_LOG}.mail-errors")
}

module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0")

action(type="omfile" file="${RSYSLOG_OUT_LOG}.messages")

ruleset(name="fileLogging") {
    if prifilt("*.info") then {
        action(type="omfile" file="${RSYSLOG_OUT_LOG}.info")
    }
}
CONF_EOF

doc_sample_expect_config_valid \
	"basic-structure" \
	"${RSYSLOG_DYNNAME}.basic-structure.conf"

exit_test
