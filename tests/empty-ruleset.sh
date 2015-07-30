#!/bin/bash
# Copyright 2014-11-20 by Rainer Gerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[empty-ruleset.sh\]: testing empty ruleset
. $srcdir/diag.sh init
. $srcdir/diag.sh startup empty-ruleset.conf
. $srcdir/diag.sh tcpflood -p13515 -m5000 -i0 # these should NOT show up
. $srcdir/diag.sh tcpflood -p13514 -m10000 -i5000
. $srcdir/diag.sh tcpflood -p13515 -m500 -i15000 # these should NOT show up
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown
. $srcdir/diag.sh seq-check 5000 14999
. $srcdir/diag.sh exit
