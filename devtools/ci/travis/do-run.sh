#!/bin/bash

if [ "$TRAVIS_EVENT_TYPE" == "cron" ] &&  [ "$DO_CRON" != "YES" ]; then
	echo cron job not executed under non-cron run
	exit 0 # this must not run under PRs
fi

export RSYSLOG_HOME=`pwd`
echo running tests/travis/$RUN, home: $RSYSLOG_HOME
$RSYSLOG_HOME/tests/travis/$RUN
