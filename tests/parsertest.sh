echo TEST: \[parsertest.sh\]: various parser tests
source $srcdir/diag.sh init
source $srcdir/diag.sh nettester parse1 udp
source $srcdir/diag.sh nettester parse1 tcp
source $srcdir/diag.sh nettester parse2 udp
source $srcdir/diag.sh nettester parse2 tcp
source $srcdir/diag.sh nettester parse_8bit_escape udp
source $srcdir/diag.sh nettester parse_8bit_escape tcp
source $srcdir/diag.sh nettester parse3 udp
source $srcdir/diag.sh nettester parse3 tcp
source $srcdir/diag.sh nettester parse_invld_regex udp
source $srcdir/diag.sh nettester parse_invld_regex tcp
source $srcdir/diag.sh nettester parse-3164-buggyday udp
source $srcdir/diag.sh nettester parse-3164-buggyday tcp
source $srcdir/diag.sh nettester parse-nodate udp
source $srcdir/diag.sh nettester parse-nodate tcp
# the following samples can only be run over UDP as they are so
# malformed they break traditional syslog/tcp framing...
source $srcdir/diag.sh nettester snare_ccoff_udp udp
source $srcdir/diag.sh nettester snare_ccoff_udp2 udp

echo \[parsertest.sh]: redoing tests in IPv4-only mode
source $srcdir/diag.sh nettester parse1 udp -4
source $srcdir/diag.sh nettester parse1 tcp -4
source $srcdir/diag.sh nettester parse2 udp -4
source $srcdir/diag.sh nettester parse2 tcp -4
source $srcdir/diag.sh nettester parse_8bit_escape udp -4
source $srcdir/diag.sh nettester parse_8bit_escape tcp -4
source $srcdir/diag.sh nettester parse3 udp -4
source $srcdir/diag.sh nettester parse3 tcp -4
source $srcdir/diag.sh nettester parse_invld_regex udp -4
source $srcdir/diag.sh nettester parse_invld_regex tcp -4
source $srcdir/diag.sh nettester parse-3164-buggyday udp -4
source $srcdir/diag.sh nettester parse-3164-buggyday tcp -4
source $srcdir/diag.sh nettester parse-nodate udp -4
source $srcdir/diag.sh nettester parse-nodate tcp -4
# UDP-only tests
source $srcdir/diag.sh nettester snare_ccoff_udp udp -4
source $srcdir/diag.sh nettester snare_ccoff_udp2 udp -4

source $srcdir/diag.sh exit
