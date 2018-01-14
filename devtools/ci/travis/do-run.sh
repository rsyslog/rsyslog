#!/bin/bash
export RSYSLOG_HOME=`pwd`
echo running tests/travis/$RUN, home: $RSYSLOG_HOME
$RSYSLOG_HOME/tests/travis/$RUN
