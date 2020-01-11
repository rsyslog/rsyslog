#!/bin/bash
# This scripts uses an rsyslog development container to execute given
# command inside it.
# Note: command line parameters are passed as parameters to the container,
# with the notable exception that -ti, if given as first parameter, is
# passed to "docker run" itself but NOT the container.
#
# use env var DOCKER_RUN_EXTRA_OPTS to provide extra options to docker run
# command.
set -e
if [ "$1" == "--rm" ]; then
	optrm="--rm"
	shift 1
fi
if [ "$1" == "-ti" ]; then
	ti="-ti"
	shift 1
fi
# check in case -ti was in front...
if [ "$1" == "--rm" ]; then
	optrm="--rm"
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
docker run $ti $optrm $DOCKER_RUN_EXTRA_OPTS \
	-u $(id -u):$(id -g) \
	-e RSYSLOG_CONFIGURE_OPTIONS_EXTRA \
	-e RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE \
	-e CC \
	-e CFLAGS \
	-e LDFLAGS \
	-e CI_MAKE_OPT \
	-e CI_MAKE_CHECK_OPT \
	-e CI_CHECK_CMD \
	-e CI_BUILD_URL \
	-e CI_CODECOV_TOKEN \
	-e CI_VALGRIND_SUPPRESSIONS \
	-e USE_AUTO_DEBUG \
	-e RSYSLOG_STATSURL \
	-e VCS_SLUG \
	--cap-add SYS_ADMIN \
	--cap-add SYS_PTRACE \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog $RSYSLOG_DEV_CONTAINER $*
