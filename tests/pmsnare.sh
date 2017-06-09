#!/bin/bash
# pmsnare.sh
# Performs parser testing for the pmsnare module.
# It's based on rgerhards' parsertest.sh.

echo TEST: \[pmsnare.sh\]: test snare parser module
. $srcdir/diag.sh init

# first we need to obtain the hostname as rsyslog sees it
rm -f HOSTNAME
. $srcdir/diag.sh startup gethostname.conf
. $srcdir/diag.sh tcpflood -m1 -M "\"<128>\""
./msleep 100
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!

# now start the real tests
. $srcdir/diag.sh nettester pmsnare_default udp
. $srcdir/diag.sh nettester pmsnare_default tcp
. $srcdir/diag.sh nettester pmsnare_ccoff udp
. $srcdir/diag.sh nettester pmsnare_ccoff tcp
. $srcdir/diag.sh nettester pmsnare_ccdefault udp
. $srcdir/diag.sh nettester pmsnare_ccdefault tcp
. $srcdir/diag.sh nettester pmsnare_cccstyle udp
. $srcdir/diag.sh nettester pmsnare_cccstyle tcp
. $srcdir/diag.sh nettester pmsnare_ccbackslash udp
. $srcdir/diag.sh nettester pmsnare_ccbackslash tcp
. $srcdir/diag.sh nettester pmsnare_modoverride udp
. $srcdir/diag.sh nettester pmsnare_modoverride tcp

echo \[pmsnare.sh]: redoing tests in IPv4-only mode
. $srcdir/diag.sh nettester pmsnare_default udp
. $srcdir/diag.sh nettester pmsnare_default tcp
. $srcdir/diag.sh nettester pmsnare_ccoff udp -4
. $srcdir/diag.sh nettester pmsnare_ccoff tcp -4
. $srcdir/diag.sh nettester pmsnare_ccdefault udp -4
. $srcdir/diag.sh nettester pmsnare_ccdefault tcp -4
. $srcdir/diag.sh nettester pmsnare_cccstyle udp -4
. $srcdir/diag.sh nettester pmsnare_cccstyle tcp -4
. $srcdir/diag.sh nettester pmsnare_ccbackslash udp -4
. $srcdir/diag.sh nettester pmsnare_ccbackslash tcp -4
. $srcdir/diag.sh nettester pmsnare_modoverride udp -4
. $srcdir/diag.sh nettester pmsnare_modoverride tcp -4
rm -f HOSTNAME
. $srcdir/diag.sh exit
