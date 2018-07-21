#!/bin/bash
echo \[lmsig_ksi_ls12.sh\]: test ksi_ls12

rm -rf $srcdir/ksitest

. $srcdir/diag.sh init
startup lmsig_ksi_ls12.conf

#generate 100 messages
for i in {0..100}; do logger -d -u /tmp/testbench_socket "test log line $i"; done

# the sleep below is needed to prevent too-early termination of rsyslogd
./msleep 100
shutdown_when_empty # shut down rsyslogd when done processing messages
./msleep 2000
wait_shutdown	# we need to wait until rsyslogd is finished!

#compare resulting files
cmp ksitest/messages.logsig.parts/block-signatures.dat resultdata/lmsig_ksi_ls12_async/messages.logsig.parts/block-signatures.dat
if [ ! $? -eq 0 ]; then
  echo "lmsig_ksi_ls12.sh failed: block files differ"
  exit 1
fi;

cmp ksitest/messages.logsig.parts/blocks.dat resultdata/lmsig_ksi_ls12_async/messages.logsig.parts/blocks.dat
if [ ! $? -eq 0 ]; then
  echo "lmsig_ksi_ls12.sh failed: signature files differ"
  exit 1
fi;

exit_test

