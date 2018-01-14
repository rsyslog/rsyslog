#!/bin/bash
# This scripts uses an rsyslog development container to execute given
# command inside it.

set -e
DEV_CONTAINER=`cat $RSYSLOG_HOME/devtools/default_dev_container`

printf "/rsyslog is mapped to $RSYSLOG_HOME\n"
docker pull $DEV_CONTAINER
docker run -e RSYSLOG_CONFIGURE_OPTIONS_EXTRA \
	-v "$RSYSLOG_HOME":/rsyslog -ti $DEV_CONTAINER  $*
