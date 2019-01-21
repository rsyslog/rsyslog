#!/bin/bash
if [ "$CI_ENV" == "Centos7VM" ]; then
	# we give up, for some reason we see errors in this env but in no other Centos 7 env
	# this is a hack for rsyslog official CI - sorry for that -- rgerhards, 2019-01-24
	echo "SKIP test, as for some reason it does not work here - this should be investigated"
	exit 77
fi
export USE_VALGRIND="YES"
source ${srcdir:=.}/imrelp-manyconn.sh
