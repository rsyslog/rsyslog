# This is a test-aid script to try running some tests through many iterations.
# It is not yet used in the automated testbench, but I keep this file so that
# I can use it whenever there is need to. As such, it currently does not have
# parameters but is expected to be edited as needed. -- rgerhards, 2010-03-10
MAXRUNS=10
DISPLAYALIVE=100
LOGFILE=runlog

date > $LOGFILE

for (( i=0; $i < 10000; i++ ))
  do
    if [ $(( i % DISPLAYALIVE )) -eq 0 ]; then
      echo "$i iterations done"
    fi
    ./gzipwr_large_dynfile.sh >> $LOGFILE
    if [ "$?" -ne "0" ]; then
      echo "Test failed in iteration $i!"
      exit 1
    fi
  done
echo "No failure in $i iterations."
