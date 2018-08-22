#!/bin/bash
# This script shall be executed before the real CI run to make sure
# the environment is "clean enough". It is not necessary in containers,
# where we usually get a fresh container for each run.
echo clean all instances of ./mmkubernetes_test_server.py that might still be running
ps -ef | grep '[0-9] python .*\./mmkubernetes_test_server.py'
for pid in $(ps -eo pid,cmd|grep '[0-9] python .*\./mmkubernetes_test_server.py' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
do
	echo "killing pid $pid"
	kill -9 $pid
done
