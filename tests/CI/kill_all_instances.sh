#!/bin/bash
# This script shall be executed before the real CI run to make sure
# the environment is "clean enough". In the past we have seen some
# left-over instances on some (buildbot) systems
echo clean all instances that might still be running
ps -ef | grep [r]syslogd
for pid in $(ps -eo pid,cmd|grep '/tools/[r]syslogd' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "killing pid $pid"
	kill -9 $pid
done
