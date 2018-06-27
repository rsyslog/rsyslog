#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Same test than 'omprog-defaults.sh', but checking for memory
# problems using valgrind. Note it is not necessary to repeat the
# rest of checks (this simplifies the maintenance of the tests).

. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
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
. $srcdir/diag.sh startup-vg
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh wait-queueempty
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh exit
