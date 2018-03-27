#!/bin/bash
# Tests for processing of partial lines in read mode 0
# This is part of the rsyslog testbench, licensed under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile" File="./rsyslog.input" Tag="file:" ReadMode="0")

template(name="outfmt" type="list") {
	constant(value="HEADER ")
	property(name="msg" format="json")
	constant(value="\n")
}

if $msg contains "msgnum:" then
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
'
. $srcdir/diag.sh startup-vg

printf 'msgnum:0
 msgnum:1' > rsyslog.input
printf '\nmsgnum:2' >> rsyslog.input

# sleep a little to give rsyslog a chance to process unterminated linet 
./msleep 500

# write some more lines (see https://github.com/rsyslog/rsyslog/issues/144)
printf 'msgnum:3
 msgnum:4' >> rsyslog.input
printf '\nmsgnum:5' >> rsyslog.input # this one shouldn't be written to the output file because of missing LF

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown-vg    # we need to wait until rsyslogd is finished!
. $srcdir/diag.sh check-exit-vg

## check if we have the correct number of messages
NUMLINES=$(grep -c HEADER rsyslog.out.log 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: expecting at least a match for HEADER, maybe rsyslog.out.log wasn't even written?"
  cat rsyslog.out.log
  . $srcdir/diag.sh error-exit 1
else
  if [ ! $NUMLINES -eq 4 ]; then
    echo "ERROR: expecting 4 headers, got $NUMLINES"
    cat rsyslog.out.log
    . $srcdir/diag.sh error-exit 1
  fi
fi

## check if all the data we expect to get in the file is there
for i in {1..4}; do
  grep msgnum:$i rsyslog.out.log > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string 'msgnum:$i', it's not there"
    cat rsyslog.out.log
    . $srcdir/diag.sh error-exit 1
  fi
done


. $srcdir/diag.sh exit
