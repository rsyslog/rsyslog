#!/bin/bash
# This scripts uses an rsyslog development container to execute given
# command inside it.
# Note: command line parameters are passed as parameters to the container,
# with the notable exception that -ti, if given as first parameter, is
# passed to "docker run" itself but NOT the container.
set -e

if [ "$1" == "-ti" ]; then
	ti="-ti"
	shift 1
fi

if [ "$RSYSLOG_HOME" == "" ]; then
	export RSYSLOG_HOME=$(pwd)
	echo info: RSYSLOG_HOME not set, using $RSYSLOG_HOME
fi

if [ -z "$RSYSLOG_DEV_CONTAINER" ]; then
	RSYSLOG_DEV_CONTAINER=$(cat $RSYSLOG_HOME/devtools/default_dev_container)
fi

printf "/rsyslog is mapped to $RSYSLOG_HOME\n"
printf "pulling container...\n"
docker pull $RSYSLOG_DEV_CONTAINER
docker run $ti \
	-u $(id -u):$(id -g) \
	-e RSYSLOG_CONFIGURE_OPTIONS_EXTRA \
	-e CC \
	-e CFLAGS \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog $RSYSLOG_DEV_CONTAINER $*
