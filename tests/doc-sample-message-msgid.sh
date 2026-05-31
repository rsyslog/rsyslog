#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-msgid.rst.
# The oracle is rsyslogd -C -N1 accepting the page's msgid template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-msgid.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="msgid")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-msgid" "${RSYSLOG_DYNNAME}.message-msgid.conf"
exit_test
