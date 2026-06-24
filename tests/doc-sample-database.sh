#!/bin/bash
# Validate the database tutorial examples from doc/source/tutorials/database.rst.
# The oracle is rsyslogd -C -N1 accepting the published template/module/action
# snippets with build-tree plugin paths, proving the tutorial still parses.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

require_plugin ommysql
doc_sample_require_loadable_module "../plugins/ommysql/.libs/ommysql.so" "doc-sample-database"

cat > "${RSYSLOG_DYNNAME}.database.conf" <<CONF_EOF
template(
    name="sqlInsertMessage"
    type="list"
    option.sql="on"
) {
    constant(value="insert into syslog(message) values ('")
    property(name="msg")
    constant(value="')")
}

module(load="../plugins/ommysql/.libs/ommysql")

action(
    type="ommysql"
    server="database-server"
    db="database-name"
    uid="database-userid"
    pwd="database-password"
)

action(
    type="ommysql"
    server="127.0.0.1"
    db="Syslog"
    uid="syslogwriter"
    pwd="topsecret"
)

if prifilt("mail.*") then {
    action(
        type="ommysql"
        server="127.0.0.1"
        db="Syslog"
        uid="syslogwriter"
        pwd="topsecret"
    )
}
CONF_EOF

doc_sample_expect_config_valid \
	"database" \
	"${RSYSLOG_DYNNAME}.database.conf"

exit_test
