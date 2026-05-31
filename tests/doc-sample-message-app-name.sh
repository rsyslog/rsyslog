#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-app-name.rst.
# The oracle is rsyslogd -C -N1 accepting the page's app-name template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-app-name.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="app-name")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-app-name" "${RSYSLOG_DYNNAME}.message-app-name.conf"
exit_test
