#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# This test mimics the test imfile-readmode2.sh, but works via
# endmsg.regex. It's kind of a base test for the regex functionality.
echo ======================================================================
# Check if inotify header exist
echo [imfile-discard-truncated-line.sh]
. $srcdir/diag.sh check-inotify
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
$MaxMessageSize 128
module(load="../plugins/imfile/.libs/imfile")
input(type="imfile"
      File="./rsyslog.input"
      discardTruncatedMsg="on"
      msgDiscardingError="off"
      Tag="file:"
      startmsg.regex="^[^ ]"
      ruleset="ruleset")
template(name="outfmt" type="list") {
  constant(value="HEADER ")
  property(name="msg" format="json")
  constant(value="\n")
}
ruleset(name="ruleset") {
	action(type="omfile" file="rsyslog.out.log" template="outfmt")
}
action(type="omfile" file="rsyslog2.out.log" template="outfmt")
'
. $srcdir/diag.sh startup

# write the beginning of the file
echo 'msgnum:0
msgnum:1
msgnum:2 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
 msgnum:3 bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
 msgnum:4 cccccccccccccccccccccccccccccccccccccccccccc
 msgnum:5 dddddddddddddddddddddddddddddddddddddddddddd
msgnum:6 eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
 msgnum:7 ffffffffffffffffffffffffffffffffffffffffffff
 msgnum:8 gggggggggggggggggggggggggggggggggggggggggggg
msgnum:9' > rsyslog.input
# the next line terminates our test. It is NOT written to the output file,
# as imfile waits whether or not there is a follow-up line that it needs
# to combine.
echo 'END OF TEST' >> rsyslog.input
# sleep a little to give rsyslog a chance to begin processing
./msleep 500
. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
. $srcdir/diag.sh wait-shutdown    # we need to wait until rsyslogd is finished!

printf 'HEADER msgnum:0
HEADER msgnum:1
HEADER msgnum:2 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\\\n msgnum:3 bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\\\\n msgnum:4 ccccccc
HEADER msgnum:6 eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\\\\n msgnum:7 ffffffffffffffffffffffffffffffffffffffffffff\\\\n msgnum:8 ggggggg
HEADER msgnum:9\n' | cmp - rsyslog.out.log
if [ ! $? -eq 0 ]; then
  echo "invalid multiline message generated, rsyslog.out.log is:"
  cat rsyslog.out.log
  exit 1
fi;

grep "imfile error:.*rest of message will not be processed" rsyslog2.out.log > /dev/null
if [ $? -eq 0 ]; then
        echo
        echo "FAIL: expected error message from missing input file not found. rsyslog2.out.log is:"
        cat rsyslog2.out.log
        . $srcdir/diag.sh error-exit 1
fi

. $srcdir/diag.sh exit
