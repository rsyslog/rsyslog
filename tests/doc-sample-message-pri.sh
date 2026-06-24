#!/bin/bash
# Validate the property template example from
# doc/source/reference/properties/message-pri.rst.
# The oracle is rsyslogd -C -N1 accepting the page's pri template snippet.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-pri.conf" <<'CONF_EOF'
template(name="example" type="list") {
    property(name="pri")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-pri" "${RSYSLOG_DYNNAME}.message-pri.conf"
exit_test
