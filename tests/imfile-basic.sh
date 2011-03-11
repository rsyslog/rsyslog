# This is part of the rsyslog testbench, licensed under GPLv3
echo [imfile-basic.sh]
source $srcdir/diag.sh init
# generate input file first. Note that rsyslog processes it as
# soon as it start up (so the file should exist at that point).
./inputfilegen 50000 > rsyslog.input
ls -l rsyslog.input
source $srcdir/diag.sh startup imfile-basic.conf
# sleep a little to give rsyslog a chance to begin processing
sleep 1
source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
source $srcdir/diag.sh seq-check 0 49999
source $srcdir/diag.sh exit
