#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

uname
if [ `uname` = "FreeBSD" ] ; then
   echo "This test currently does not work on FreeBSD."
   exit 77
fi

echo ======================================================================
echo [imfile-endregex.sh]
. $srcdir/diag.sh init
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")

input(type="imfile"
      File="./rsyslog.input"
      Tag="file:"
      startmsg.regex="^[^ ]")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="\n")
}

if $msg contains "msgnum:" then
 action(
   type="omfile"
   file=`echo $RSYSLOG_OUT_LOG`
   template="outfmt"
 )
'
startup_vg

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

shutdown_when_empty # shut down rsyslogd when done processing messages
wait_shutdown_vg    # we need to wait until rsyslogd is finished!
. $srcdir/diag.sh check-exit-vg

# give it time to write the output file
sleep 1

## check if we have the correct number of messages

NUMLINES=$(grep -c HEADER  $RSYSLOG_OUT_LOG 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: expecting at least a match for HEADER, maybe  $RSYSLOG_OUT_LOG wasn't even written?"
  exit 1
else
  if [ ! $NUMLINES -eq 3 ]; then
    echo "ERROR: expecting 3 headers, got $NUMLINES"
    exit 1
  fi
fi

## check if all the data we expect to get in the file is there

for i in {1..4}; do
  grep msgnum:$i  $RSYSLOG_OUT_LOG > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string 'msgnum:$i', it's not there"
    exit 1
  fi
done

## if we got here, all is good :)

exit_test
