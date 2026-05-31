#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-syslogtag.rst.
# The oracle is rsyslogd -C -N1 accepting the page's syslogtag template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-syslogtag.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="syslogtag")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-syslogtag" "${RSYSLOG_DYNNAME}.message-syslogtag.conf"
exit_test
