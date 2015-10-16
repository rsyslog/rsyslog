#!/bin/bash
# Test if rsyslog survives sending truely random data to it...
#
# added 2010-04-01 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo TEST: \[random.sh\]: testing random data
. $srcdir/diag.sh init
. $srcdir/diag.sh startup random.conf
# generate random data
./randomgen -f rsyslog.random.data -s 100000
ls -l rsyslog.random.data
. $srcdir/diag.sh tcpflood -B -I rsyslog.random.data -c5 -C10
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown       # and wait for it to terminate
# we do not check anything yet, the point is if rsyslog survived ;)
# TODO: check for exit message, but we'll notice an abort anyhow, so not that important
rm -f random.data
. $srcdir/diag.sh exit
