echo TEST: \[parsertest.sh\]: various parser tests
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester parse1 udp
source $srcdir/diag.sh nettester parse1 tcp
source $srcdir/diag.sh nettester parse2 udp
source $srcdir/diag.sh nettester parse2 tcp

echo \[parsertest.sh]: redoing tests in IPv4-only mode
source $srcdir/diag.sh nettester parse1 udp -4
source $srcdir/diag.sh nettester parse1 tcp -4
source $srcdir/diag.sh nettester parse2 udp -4
source $srcdir/diag.sh nettester parse2 tcp -4
source $srcdir/diag.sh init
