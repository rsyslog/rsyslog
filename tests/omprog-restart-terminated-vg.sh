#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Same test than 'omprog-restart-terminated.sh', but checking for memory
# problems using valgrind. Note it is not necessary to repeat the
# rest of checks (this simplifies the maintenance of the tests).

. $srcdir/diag.sh init
startup_vg omprog-restart-terminated.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg 0 1
. $srcdir/diag.sh wait-queueempty

. $srcdir/diag.sh injectmsg 1 1
. $srcdir/diag.sh injectmsg 2 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 3 1
. $srcdir/diag.sh injectmsg 4 1
. $srcdir/diag.sh wait-queueempty

pkill -TERM -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 5 1
. $srcdir/diag.sh injectmsg 6 1
. $srcdir/diag.sh injectmsg 7 1
. $srcdir/diag.sh wait-queueempty

pkill -USR1 -f omprog-restart-terminated-bin.sh
sleep .1

. $srcdir/diag.sh injectmsg 8 1
. $srcdir/diag.sh injectmsg 9 1
. $srcdir/diag.sh wait-queueempty

shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
