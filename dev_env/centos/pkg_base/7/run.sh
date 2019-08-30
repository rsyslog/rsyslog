#!/bin/bash
printf "\n\n================== ENTER DOCKER CONTAINER\n"
MYBASEDIR=$(pwd)
# Run private ENV variables
source ./private/private-env.sh

# Run docker
docker run \
	--privileged \
	--cap-add=SYS_ADMIN \
	-e PKGBASEDIR \
	-e REPOUSERNAME \
	-e REPOURL \
	-v "$MYBASEDIR/private/mount":/private-files \
	-v "$MYBASEDIR/yumrepo/mount":/home/pkg/rsyslog-pkg-rhel-centos/yumrepo \
	-ti rsyslog/rsyslog_dev_pkg_base_centos:7
