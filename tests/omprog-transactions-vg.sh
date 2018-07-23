#!/bin/bash
# This file is part of the rsyslog project, released under ASL 2.0

# Same test than 'omprog-transactions.sh', but checking for memory
# problems using valgrind. Note it is not necessary to repeat the
# rest of checks (this simplifies the maintenance of the tests).

. $srcdir/diag.sh init
startup_vg omprog-transactions.conf
. $srcdir/diag.sh wait-startup
. $srcdir/diag.sh injectmsg 0 10
. $srcdir/diag.sh wait-queueempty
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg
exit_test
