# This is part of the rsyslog testbench, licensed under ASL 2.0
echo ======================================================================
echo [imfile-readmode2.sh]
source $srcdir/diag.sh init
source $srcdir/diag.sh startup imfile-readmode2.conf

# write the beginning of the file
echo 'msgnum:0
 msgnum:1' > rsyslog.input
echo 'msgnum:2' >> rsyslog.input

# sleep a little to give rsyslog a chance to begin processing
sleep 1

# write some more lines (see https://github.com/rsyslog/rsyslog/issues/144)
echo 'msgnum:3
 msgnum:4' >> rsyslog.input
echo 'msgnum:5' >> rsyslog.input # this one shouldn't be written to the output file because of ReadMode 2

# give it time to finish
sleep 1

source $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
source $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

# give it time to write the output file
sleep 1

## check if we have the correct number of messages

NUMLINES=$(grep -c HEADER $srcdir/rsyslog.out.log 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: expecting at least a match for HEADER, maybe rsyslog.out.log wasn't even written?"
  source $srcdir/diag.sh exit
  exit 1
else
  if [ ! $NUMLINES -eq 3 ]; then
    echo "ERROR: expecting 3 headers, got $NUMLINES"
    source $srcdir/diag.sh exit
    exit 1
  fi
fi

## check if all the data we expect to get in the file is there

for i in {1..4}; do
  grep msgnum:$i $srcdir/rsyslog.out.log > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string 'msgnum:$i', it's not there"
    source $srcdir/diag.sh exit
    exit 1
  fi
done

## if we got here, all is good :)

source $srcdir/diag.sh exit
