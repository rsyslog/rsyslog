#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-inputname.rst.
# The oracle is rsyslogd -C -N1 accepting the page's inputname template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-inputname.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="inputname")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-inputname" "${RSYSLOG_DYNNAME}.message-inputname.conf"
exit_test
