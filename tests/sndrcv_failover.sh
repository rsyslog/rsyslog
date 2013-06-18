# This tests failover capabilities. Data is sent to local port 13516, where
# no process shall listen. Then it fails over to a second instance, then to
# a file. The second instance is started. So all data should be received
# there and none be logged to the file.
# This builds on the basic sndrcv.sh test, but adds a first, failing,
# location to the conf file.
# added 2011-06-20 by Rgerhards
# This file is part of the rsyslog project, released  under GPLv3
echo ===============================================================================
echo \[sndrcv_failover.sh\]: testing failover capabilities for tcp sending
source $srcdir/sndrcv_drvr_noexit.sh sndrcv_failover 50000
ls -l rsyslog.empty
if [[ -s rsyslog.empty ]] ; then
  echo "FAIL: rsyslog.empty has data. Failover handling failed. Data is written"
  echo "      even though the previous action (in a failover chain!) properly"
  echo "      worked."
  exit 1
else
  echo "rsyslog.empty is empty - OK"
fi ;
source $srcdir/diag.sh exit
