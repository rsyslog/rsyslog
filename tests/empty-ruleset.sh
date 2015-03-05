# Copyright 2014-11-20 by Rainer Gerhards
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[empty-ruleset.sh\]: testing empty ruleset
source $srcdir/diag.sh init
source $srcdir/diag.sh startup empty-ruleset.conf
source $srcdir/diag.sh tcpflood -p13515 -m5000 -i0 # these should NOT show up
source $srcdir/diag.sh tcpflood -p13514 -m10000 -i5000
source $srcdir/diag.sh tcpflood -p13515 -m500 -i15000 # these should NOT show up
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown
source $srcdir/diag.sh seq-check 5000 14999
source $srcdir/diag.sh exit
