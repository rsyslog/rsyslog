#!/bin/bash
# This is a test-aid script to try running some tests through many iterations.
# It is not yet used in the automated testbench, but I keep this file so that
# I can use it whenever there is need to. As such, it currently does not have
# parameters but is expected to be edited as needed. -- rgerhards, 2010-03-10
#
# use: ./longrun.sh testname.sh
#
# where testname.sh is the test to be run
# to change other params, you need to edit the settings here below:
MAXRUNS=10
DISPLAYALIVE=100
LOGFILE=runlog

echo "logfile is $LOGFILE"
echo "executing test $1"

date > $LOGFILE

for (( i=0; $i < 10000; i++ ))
  do
    if [ $(( i % DISPLAYALIVE )) -eq 0 ]; then
      echo "$i iterations done"
    fi
    $1 >> $LOGFILE
    if [ "$?" -ne "0" ]; then
      echo "Test failed in iteration $i, review $LOGFILE for details!"
      exit 1
    fi
  done
echo "No failure in $i iterations."
