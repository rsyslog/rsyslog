#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Same test than 'omprog-defaults.sh', but checking for memory
# problems using valgrind. Note it is not necessary to repeat the
# rest of checks (this simplifies the maintenance of the tests).

. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/omprog/.libs/omprog")

template(name="outfmt" type="string" string="%msg%\n")

:msg, contains, "msgnum:" {
    action(
        type="omprog"
	binary=`echo $srcdir/testsuites/omprog-defaults-bin.sh param1 param2 param3`
        template="outfmt"
        name="omprog_action"
    )
}
'
startup_vg
injectmsg 0 10
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check_exit_vg
exit_test
