#!/bin/bash
# Validate the larger template and rule examples from
# doc/source/configuration/examples.rst.
# The oracle is rsyslogd -C -N1 accepting the concatenated page examples so the
# published template/action snippets keep matching current config syntax.
. ${srcdir:=.}/diag.sh init
. $srcdir/doc-sample-lib.sh

cat > "${RSYSLOG_DYNNAME}.config-examples.conf" <<CONF_EOF
template(name="TraditionalFormat" type="list") {
    property(name="timegenerated")
    constant(value=" ")
    property(name="hostname")
    property(name="syslogtag")
    property(name="msg" droplastlf="on")
    constant(value="\n")
}

template(name="PreciseFormat" type="list") {
    property(name="syslogpriority")
    constant(value=",")
    property(name="syslogfacility")
    constant(value=",")
    property(name="timegenerated")
    constant(value=",")
    property(name="hostname")
    constant(value=",")
    property(name="syslogtag")
    constant(value=",")
    property(name="msg")
    constant(value="\n")
}

template(name="RFC3164fmt" type="list") {
    constant(value="<")
    property(name="pri")
    constant(value=">")
    property(name="timestamp")
    constant(value=" ")
    property(name="hostname")
    constant(value=" ")
    property(name="syslogtag")
    property(name="msg")
}

template(name="MySQLInsert" type="list" option.sql="on") {
    constant(value="insert iut, message, receivedat values ('")
    property(name="iut")
    constant(value="', '")
    property(name="msg" caseconversion="upper")
    constant(value="', '")
    property(name="timegenerated" dateformat="mysql")
    constant(value="') into systemevents\n")
}

if prifilt("*.crit") and not prifilt("kern.*") then {
    action(type="omfile" file="${RSYSLOG_OUT_LOG}.critical")
}

if prifilt("kern.*") then {
    action(type="omfile" file="${RSYSLOG_OUT_LOG}.kernel")
}

if prifilt("kern.crit") then {
    action(
        type="omfwd"
        target="server.example.net"
        protocol="tcp"
        port="514"
        template="RFC3164fmt"
    )
}

if prifilt("*.emerg") then {
    action(type="omusrmsg" users="*")
}

action(
    type="omfwd"
    target="server.example.net"
    protocol="tcp"
    port="514"
)

if (\$msg contains "error") then {
    action(
        type="omfwd"
        target="server.example.net"
        protocol="udp"
    )
}
CONF_EOF

doc_sample_expect_config_valid \
	"config-examples" \
	"${RSYSLOG_DYNNAME}.config-examples.conf"

exit_test
