#!/bin/bash
# This script gathers and sends to stdout all log files created during
# make check. This is useful if make check is terminated due to timeout,
# in which case autotools unfortunately does not provide us a way to
# gather error information.
rm -f tests/*.sh.log zookeeper.pid
rm -rf rstb_*

# cleanup of hanging instances from previous runs
# practice has shown this is pretty useful!
for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "ERROR: left-over previous instance $pid, killing it"
	ps -fp $pid
	pwd
	kill -9 $pid
done

# now the same for mmkubernetes_test_server
for pid in $(ps -eo pid,args|grep '\./[m]mkubernetes_test_server.py ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "ERROR: left-over previous instance $pid, killing it"
	ps -fp $pid
	kill -9 $pid || exit 0
done

# now the same for kafaka
for pid in $(ps -eo pid,args|grep '[k]afka' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "ERROR: left-over previous instance $pid, killing it"
	ps -fp $pid
	kill -9 $pid || exit 0
done

# now the same for zookeeper
for pid in $(ps -eo pid,args|grep '[z]ookeeper' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "ERROR: left-over previous instance $pid, killing it"
	ps -fp $pid
	kill -9 $pid || exit 0
done
