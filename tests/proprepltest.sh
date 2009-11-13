echo \[proprepltest.sh\]: various tests for the property replacer
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester rfctag udp
source $srcdir/diag.sh nettester rfctag tcp
source $srcdir/diag.sh nettester nolimittag udp
source $srcdir/diag.sh nettester nolimittag tcp
source $srcdir/diag.sh init
