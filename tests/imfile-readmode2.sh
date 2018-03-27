#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
#export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction stdout"
#export RSYSLOG_DEBUGLOG="log"

. $srcdir/diag.sh init
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
global( debug.whitelist="on"
	debug.files=["imfile.c"])

module(load="../plugins/imfile/.libs/imfile")

input(type="imfile" File="./rsyslog.input" Tag="file:" ReadMode="2")
#input(type="imfile" File="./rsyslog.input.*" Tag="file:" ReadMode="2")

template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="\n")
}

if $msg contains "msgnum:" then
	action(type="omfile" file="rsyslog.out.log" template="outfmt"
 ) '
. $srcdir/diag.sh startup

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
#echo you are in bash! ctl-d to exit
#bash

. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

# give it time to write the output file

sleep 1
#cat log

## check if we have the correct number of messages

NUMLINES=$(grep -c HEADER ./rsyslog.out.log 2>/dev/null)

if [ -z $NUMLINES ]; then
  echo "ERROR: expecting at least a match for HEADER, maybe rsyslog.out.log wasn't even written?" cat ./rsyslog.out.log exit 1
else
  if [ ! $NUMLINES -eq 3 ]; then
    echo "ERROR: expecting 3 headers, got $NUMLINES"
    cat ./rsyslog.out.log
    exit 1
  fi
fi

## check if all the data we expect to get in the file is there

for i in {1..4}; do
  grep msgnum:$i ./rsyslog.out.log > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string 'msgnum:$i', it's not there"
    cat ./rsyslog.out.log
    exit 1
  fi
done

## if we got here, all is good :)

. $srcdir/diag.sh exit
