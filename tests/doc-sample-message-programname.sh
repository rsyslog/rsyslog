#!/bin/bash
# Validate the property examples from
# doc/source/reference/properties/message-programname.rst.
# The oracle is rsyslogd -C -N1 accepting both the global option and template snippets.

. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.message-programname.conf" <<'CONF_EOF'
global(parser.permitSlashInProgramName="on")

template(name="example" type="list") {
    property(name="programname")
}

action(type="omfile" file="/dev/null" template="example")
CONF_EOF

doc_sample_expect_config_valid "message-programname" "${RSYSLOG_DYNNAME}.message-programname.conf"
exit_test
