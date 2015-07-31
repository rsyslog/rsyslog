#!/bin/bash
echo \[proprepltest.sh\]: various tests for the property replacer
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-HOSTNAME
. $srcdir/diag.sh nettester rfctag udp
. $srcdir/diag.sh nettester rfctag tcp
. $srcdir/diag.sh nettester nolimittag udp
. $srcdir/diag.sh nettester nolimittag tcp
. $srcdir/diag.sh init
