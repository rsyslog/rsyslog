# Copyright 2015-01-29 by Tim Eifler
# This file is part of the rsyslog project, released  under ASL 2.0
echo ===============================================================================
echo \[abort-uncleancfg-goodcfg.sh\]: testing abort on unclean configuration
echo "testing a good Configuration verification run"
source $srcdir/diag.sh init
source $srcdir/diag.sh startup abort-uncleancfg-goodcfg.conf 
source $srcdir/diag.sh tcpflood -m10 -i1 
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown

if [ ! -e rsyslog.out.log ]
then
        echo "error: expected file does not exist"
	source ./diag.sh error-exit 1
fi
source $srcdir/diag.sh exit
