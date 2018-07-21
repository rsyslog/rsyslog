#!/bin/bash
# Check that invalid variable names are detected.
# Copyright 2017-01-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
generate_conf
add_conf '
template(name="json" type="string" string="%$!%\n")
ruleset(name="rcvr" queue.type="LinkedList") {
	set $@timestamp="test";
	unset $@timestamp2;
	action(type="omfile" file="rsyslog2.out.log")
}

action(type="omfile" file="rsyslog.out.log")
 
'
startup
. $srcdir/diag.sh injectmsg  0 10
shutdown_when_empty
wait_shutdown

grep "@timestamp" rsyslog.out.log > /dev/null
if [ ! $? -eq 0 ]; then
  echo "expected error message on \"@timestamp\" not found, output is:"
  echo "------------------------------------------------------------"
  cat rsyslog.out.log
  echo "------------------------------------------------------------"
  error_exit 1
fi;

grep "@timestamp2" rsyslog.out.log > /dev/null
if [ ! $? -eq 0 ]; then
  echo "expected error message on \"@timestamp2\" not found, output is:"
  echo "------------------------------------------------------------"
  cat rsyslog.out.log
  echo "------------------------------------------------------------"
  error_exit 1
fi;

exit_test
