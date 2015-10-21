#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
# Check if inotify header exist
if [ -n "$(find /usr/include -name 'inotify.h' -print -quit)" ]; then
	echo [imfile-readmode2-with-persists-data-during-stop.sh]
else
	exit 77 # no inotify available, skip this test
fi
. $srcdir/diag.sh init
. $srcdir/diag.sh startup imfile-readmode2-with-persists-data-during-stop.conf

# write the beginning of the file
echo 'msgnum:0
 msgnum:1' > rsyslog.input
echo 'msgnum:2' >> rsyslog.input

# sleep a little to give rsyslog a chance to begin processing
sleep 1

# now stop and restart rsyslog so that the file info must be
# persisted and read again on startup. Results should still be
# correct ;)
echo stopping rsyslog
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

# write some more lines - we want to check here if the initial
# polling loop properly picks up that data. Note that even in
# inotify case we do have one polling loop at startup, as this
# is required to find data written while we were stopped.

echo 'msgnum:3
 msgnum:4' >> rsyslog.input

echo restarting rsyslog
. $srcdir/diag.sh startup imfile-readmode2-with-persists.conf
echo restarted rsyslog, continuing with test

echo ' msgnum:5' >> rsyslog.input
echo 'msgnum:6
 msgnum:7
msgnum:8' >> rsyslog.input
#msgnum:8 must NOT be written as it is unfinished

# give it time to finish
sleep 1

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

# give it time to write the output file
sleep 1

## check if we have the correct number of messages

NUMLINES=$(grep -c HEADER rsyslog.out.log 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: expecting at least a match for HEADER, maybe rsyslog.out.log wasn't even written?"
  cat ./rsyslog.out.log
  exit 1
else
  if [ ! $NUMLINES -eq 4 ]; then
    echo "ERROR: expecting 4 headers, got $NUMLINES"
    cat ./rsyslog.out.log
    exit 1
  fi
fi

## check if all the data we expect to get in the file is there

for i in {1..7}; do
  grep msgnum:$i rsyslog.out.log > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string 'msgnum:$i', it's not there"
    cat ./rsyslog.out.log
    exit 1
  fi
done

## if we got here, all is good :)

. $srcdir/diag.sh exit
