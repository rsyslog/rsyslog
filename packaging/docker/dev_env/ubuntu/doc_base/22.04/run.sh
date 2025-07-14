#!/bin/bash
printf "\n\n================== ENTER DOCKER CONTAINER\n"

if [ "$RSYSLOG_HOME" == "" ]; then
        export RSYSLOG_HOME=$(pwd)
        echo info: RSYSLOG_HOME not set, using $RSYSLOG_HOME
fi

printf 'user ids: %s:%s\n' $(id -u) $(id -g)
printf 'container_uid: %s\n' ${RSYSLOG_CONTAINER_UID--u $(id -u):$(id -g)}
printf 'container cmd: %s\n' $*

# Run docker
docker run \
	--privileged \
	--cap-add=SYS_ADMIN \
	-e ENVTODO \
	${RSYSLOG_CONTAINER_UID--u $(id -u):$(id -g)} \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog \
	-ti --rm rsyslog/rsyslog_dev_doc_base_ubuntu:22.04
