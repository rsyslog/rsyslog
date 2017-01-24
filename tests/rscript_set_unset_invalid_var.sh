#!/bin/bash
# Check that invalid variable names are detected.
# Copyright 2017-01-24 by Rainer Gerhards
# This file is part of the rsyslog project, released under ASL 2.0
. $srcdir/diag.sh init
. $srcdir/diag.sh generate-conf
. $srcdir/diag.sh add-conf '
template(name="json" type="string" string="%$!%\n")
ruleset(name="rcvr" queue.type="LinkedList") {
	set $@timestamp="test";
	unset $@timestamp2;
	action(type="omfile" file="rsyslog2.out.log")
}

action(type="omfile" file="rsyslog.out.log")
 
'
. $srcdir/diag.sh startup
. $srcdir/diag.sh injectmsg  0 10
. $srcdir/diag.sh shutdown-when-empty
. $srcdir/diag.sh wait-shutdown

grep "@timestamp" rsyslog.out.log > /dev/null
if [ ! $? -eq 0 ]; then
  echo "expected error message on \"@timestamp\" not found, output is:"
  echo "------------------------------------------------------------"
  cat rsyslog.out.log
  echo "------------------------------------------------------------"
  . $srcdir/diag.sh error-exit 1
fi;

grep "@timestamp2" rsyslog.out.log > /dev/null
if [ ! $? -eq 0 ]; then
  echo "expected error message on \"@timestamp2\" not found, output is:"
  echo "------------------------------------------------------------"
  cat rsyslog.out.log
  echo "------------------------------------------------------------"
  . $srcdir/diag.sh error-exit 1
fi;

. $srcdir/diag.sh exit
