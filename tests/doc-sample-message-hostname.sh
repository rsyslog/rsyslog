#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-hostname.rst.
# The oracle is rsyslogd -C -N1 accepting the page's hostname template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-hostname.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="hostname")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-hostname" "${RSYSLOG_DYNNAME}.message-hostname.conf"
exit_test
