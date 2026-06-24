#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-msg.rst.
# The oracle is rsyslogd -C -N1 accepting the page's msg template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-msg.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="msg")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-msg" "${RSYSLOG_DYNNAME}.message-msg.conf"
exit_test
