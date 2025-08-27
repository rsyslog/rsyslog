#!/bin/bash
# this test checks truncation mode, and does so with multiple truncations of
# the input file.
# It also needs a larger load, which shall be sufficient to do begin of file
# checks as well as should support file id hash generation.
# add 2016-10-06 by RGerhards, released under ASL 2.0
. ${srcdir:=.}/diag.sh init
. $srcdir/diag.sh check-inotify
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile" pollingInterval="1")

input(type="imfile" File="'./$RSYSLOG_DYNNAME'.input" Tag="file:" reopenOnTruncate="on")

template(name="outfmt" type="string" string="%msg:F,58:2%\n")
if $msg contains "msgnum:" then
	action( type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="outfmt")
'

# write the beginning of the file
NUMMSG=1000
./inputfilegen -m 1000 -i0 > $RSYSLOG_DYNNAME.input
ls -li $RSYSLOG_DYNNAME.input
inode=$(get_inode $RSYSLOG_DYNNAME.input )
echo inode $inode

startup

for i in {0..50}; do
	# check that previous msg injection worked
	wait_file_lines  $RSYSLOG_OUT_LOG $NUMMSG 100
	seq_check 0 $((NUMMSG - 1))

	# begin new inject cycle
	generate_msgs=$(( i * 50))
	echo generating $NUMMSG .. $((NUMMSG + generate_msgs -1))
	./inputfilegen -m$generate_msgs -i$NUMMSG > $RSYSLOG_DYNNAME.input
	(( NUMMSG=NUMMSG+generate_msgs ))
	if [  $inode -ne $( get_inode $RSYSLOG_DYNNAME.input ) ]; then
		echo FAIL testbench did not keep same inode number, expected $inode
		ls -li $RSYSLOG_DYNNAME.input
		exit 100
	fi
done

echo generated $NUMMSG messages

shutdown_when_empty
wait_shutdown

seq_check 0 $((NUMMSG - 1))
exit_test
