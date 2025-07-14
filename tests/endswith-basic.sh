#!/bin/bash
. ${srcdir:=.}/diag.sh init
generate_conf
add_conf '
template(name="outfmt" type="string" string="%programname% %msg%\n")
if $programname endswith ["_foo", "-bar", ".baz"] then {
        action(type="omfile" template="outfmt" file="'${RSYSLOG_OUT_LOG}'")
}
'
startup
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host service_foo - - - test1'
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host service-bar - - - test2'
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host service.baz - - - test3'
injectmsg_literal '<165>1 2003-03-01T01:00:00.000Z host otherprog - - - test4'
shutdown_when_empty
wait_shutdown
export EXPECTED="service_foo test1
service-bar test2
service.baz test3"
cmp_exact
exit_test
