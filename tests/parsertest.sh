echo TEST: parsertest.sh - various parser tests
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester parse1 udp
source $srcdir/diag.sh nettester parse1 tcp
source $srcdir/diag.sh init
