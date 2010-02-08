echo TEST: \[parsertest.sh\]: various parser tests
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester parse1 udp
source $srcdir/diag.sh nettester parse1 tcp
source $srcdir/diag.sh nettester parse3 udp
source $srcdir/diag.sh nettester parse3 tcp
source $srcdir/diag.sh nettester parse_invld_regex udp
source $srcdir/diag.sh nettester parse_invld_regex tcp

echo \[parsertest.sh]: redoing tests in IPv4-only mode
source $srcdir/diag.sh nettester parse1 udp -4
source $srcdir/diag.sh nettester parse1 tcp -4
source $srcdir/diag.sh nettester parse3 udp -4
source $srcdir/diag.sh nettester parse3 tcp -4
source $srcdir/diag.sh nettester parse_invld_regex udp -4
source $srcdir/diag.sh nettester parse_invld_regex tcp -4
source $srcdir/diag.sh exit
