#!/bin/bash
printf "\n\n================== ENTER DOCKER CONTAINER\n"

# Run private ENV variables
source ./private/private-env.sh

# Run docker without extra SYS_ADMIN (still privileged for mock)
docker run \
	--privileged \
	-e PKGBASEDIR \
	-e REPOUSERNAME \
	-e REPOURL \
	-e REPOSSHPORT \
	-v "$PKGPRIVATEBASEDIR/private/mount":/private-files \
	-v "$PKGPRIVATEBASEDIR/yumrepo/mount":/home/pkg/rsyslog-pkg-rhel-centos/yumrepo \
	-ti --rm rsyslog/rsyslog_dev_pkg_base_rocky:9
