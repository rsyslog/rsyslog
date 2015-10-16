#!/bin/bash
echo TEST: \[parsertest.sh\]: various parser tests
. $srcdir/diag.sh init

# first we need to obtain the hostname as rsyslog sees it
rm -f HOSTNAME
. $srcdir/diag.sh startup gethostname.conf
. $srcdir/diag.sh tcpflood -m1 -M "\"<128>\""
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!

# now start the real tests
. $srcdir/diag.sh nettester parse1 udp
. $srcdir/diag.sh nettester parse1 tcp
. $srcdir/diag.sh nettester parse2 udp
. $srcdir/diag.sh nettester parse2 tcp
. $srcdir/diag.sh nettester parse_8bit_escape udp
. $srcdir/diag.sh nettester parse_8bit_escape tcp
. $srcdir/diag.sh nettester parse3 udp
. $srcdir/diag.sh nettester parse3 tcp
. $srcdir/diag.sh nettester parse_invld_regex udp
. $srcdir/diag.sh nettester parse_invld_regex tcp
. $srcdir/diag.sh nettester parse-3164-buggyday udp
. $srcdir/diag.sh nettester parse-3164-buggyday tcp
. $srcdir/diag.sh nettester parse-nodate udp
. $srcdir/diag.sh nettester parse-nodate tcp
# the following samples can only be run over UDP as they are so
# malformed they break traditional syslog/tcp framing...
. $srcdir/diag.sh nettester snare_ccoff_udp udp
. $srcdir/diag.sh nettester snare_ccoff_udp2 udp

echo \[parsertest.sh]: redoing tests in IPv4-only mode
. $srcdir/diag.sh nettester parse1 udp -4
. $srcdir/diag.sh nettester parse1 tcp -4
. $srcdir/diag.sh nettester parse2 udp -4
. $srcdir/diag.sh nettester parse2 tcp -4
. $srcdir/diag.sh nettester parse_8bit_escape udp -4
. $srcdir/diag.sh nettester parse_8bit_escape tcp -4
. $srcdir/diag.sh nettester parse3 udp -4
. $srcdir/diag.sh nettester parse3 tcp -4
. $srcdir/diag.sh nettester parse_invld_regex udp -4
. $srcdir/diag.sh nettester parse_invld_regex tcp -4
. $srcdir/diag.sh nettester parse-3164-buggyday udp -4
. $srcdir/diag.sh nettester parse-3164-buggyday tcp -4
. $srcdir/diag.sh nettester parse-nodate udp -4
. $srcdir/diag.sh nettester parse-nodate tcp -4
# UDP-only tests
. $srcdir/diag.sh nettester snare_ccoff_udp udp -4
. $srcdir/diag.sh nettester snare_ccoff_udp2 udp -4

rm -f HOSTNAME
. $srcdir/diag.sh exit
