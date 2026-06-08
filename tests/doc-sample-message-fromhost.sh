#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-fromhost.rst.
# The oracle is rsyslogd -C -N1 accepting the page's fromhost template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-fromhost.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="fromhost")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-fromhost" "${RSYSLOG_DYNNAME}.message-fromhost.conf"
exit_test
