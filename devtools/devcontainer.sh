#!/bin/bash
# This scripts uses an rsyslog development container to execute given
# command inside it.
# Note: command line parameters are passed as parameters to the container,
# with the notable exception that -ti, if given as first parameter, is
# passed to "docker run" itself but NOT the container.
#
# use env var DOCKER_RUN_EXTRA_OPTS to provide extra options to docker run
# command.
#
# TO MODIFIY BEHAVIOUR, use
# RSYSLOG_CONTAINER_UID, format uid:gid,
#                   to change the users container is run under
#                   set to "" to use the container default settings
#                   (no local mapping)
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

printf '/rsyslog is mapped to %s \n' "$RSYSLOG_HOME"
printf 'using container %s\n' "$RSYSLOG_DEV_CONTAINER"
printf 'pulling container...\n'
docker pull $RSYSLOG_DEV_CONTAINER
printf 'user ids: %s:%s\n' $(id -u) $(id -g)
printf 'container_uid: %s\n' ${RSYSLOG_CONTAINER_UID--u $(id -u):$(id -g)}
printf 'container cmd: %s\n' "$*"
docker run $ti $optrm $DOCKER_RUN_EXTRA_OPTS \
	-e RSYSLOG_CONFIGURE_OPTIONS_EXTRA \
	-e RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE \
	-e CC \
	-e CFLAGS \
	-e LDFLAGS \
	-e LSAN_OPTIONS \
	-e TSAN_OPTIONS \
	-e UBSAN_OPTIONS \
	-e CI_MAKE_OPT \
	-e CI_MAKE_CHECK_OPT \
	-e CI_CHECK_CMD \
	-e CI_BUILD_URL \
	-e CI_CODECOV_TOKEN \
	-e CI_VALGRIND_SUPPRESSIONS \
	-e CI_SANITIZE_BLACKLIST \
	-e ABORT_ALL_ON_TEST_FAIL \
	-e USE_AUTO_DEBUG \
	-e RSYSLOG_STATSURL \
	-e RSYSLOG_TESTBENCH_EXTERNAL_ES_URL \
	-e RSYSLOG_TESTBENCH_EXTERNAL_ES_HOST \
	-e RSYSLOG_TESTBENCH_EXTERNAL_ES_PORT \
	-e CODECOV_commit_sha \
	-e CODECOV_repo_slug \
	-e VCS_SLUG \
	-e VERBOSE \
	--cap-add SYS_ADMIN \
	--cap-add SYS_PTRACE \
	${RSYSLOG_CONTAINER_UID--u $(id -u):$(id -g)} \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog $RSYSLOG_DEV_CONTAINER "$@"
