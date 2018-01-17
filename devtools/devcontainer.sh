#!/bin/bash
# This scripts uses an rsyslog development container to execute given
# command inside it.
set -e

if [ "$RSYSLOG_HOME" == "" ]; then
	export RSYSLOG_HOME=$(pwd)
	echo info: RSYSLOG_HOME not set, using $RSYSLOG_HOME
fi

if [ -z "$RSYSLOG_DEV_CONTAINER" ]; then
	RSYSLOG_DEV_CONTAINER=`cat $RSYSLOG_HOME/devtools/default_dev_container`
fi

printf "/rsyslog is mapped to $RSYSLOG_HOME\n"
docker pull $RSYSLOG_DEV_CONTAINER
docker run \
	-u `id -u`:`id -g` \
	-e RSYSLOG_CONFIGURE_OPTIONS_EXTRA \
	-e CC \
	-e CFLAGS \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog $RSYSLOG_DEV_CONTAINER $*
