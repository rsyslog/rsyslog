#!/bin/bash
# added 2015-09-30 by singh.janmejay
# This file is part of the rsyslog project, released under ASL 2.0
echo ===============================================================================
echo \[unused_lookup_table.sh\]: test for ensuring clean destruction of lookup-table even when it is never used
. $srcdir/diag.sh init
cp $srcdir/testsuites/xlate.lkp_tbl $srcdir/xlate.lkp_tbl
. $srcdir/diag.sh startup-vg unused_lookup_table.conf
. $srcdir/diag.sh injectmsg  0 1
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown-vg
. $srcdir/diag.sh check-exit-vg
. $srcdir/diag.sh content-check "msgnum:00000000:"
. $srcdir/diag.sh exit

# the test actually expects clean destruction of lookup_table
# when lookup_table is loaded, it can either be:
# - used (clean destruct covered by another test)
# - not-used (this test ensures its destroyed cleanly)
