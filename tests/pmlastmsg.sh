#!/bin/bash
echo ==============================================================================
echo \[pmlastmsg.sh\]: tests for pmlastmsg
. $srcdir/diag.sh init
. $srcdir/diag.sh nettester pmlastmsg udp
. $srcdir/diag.sh nettester pmlastmsg tcp
. $srcdir/diag.sh exit
