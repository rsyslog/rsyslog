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
	RSYSLOG_HOME=$(pwd)
	export RSYSLOG_HOME
	echo info: RSYSLOG_HOME not set, using "$RSYSLOG_HOME"
fi

if [ -z "$RSYSLOG_DEV_CONTAINER" ]; then
	RSYSLOG_DEV_CONTAINER=$(cat "$RSYSLOG_HOME"/devtools/default_dev_container)
fi

printf '/rsyslog is mapped to %s \n' "$RSYSLOG_HOME"
printf 'using container %s\n' "$RSYSLOG_DEV_CONTAINER"
printf 'pulling container...\n'
docker pull "$RSYSLOG_DEV_CONTAINER"
container_uid_args=
container_uid_spec=
if [ "${RSYSLOG_CONTAINER_UID+x}" ]; then
	if [ -n "$RSYSLOG_CONTAINER_UID" ]; then
		case "$RSYSLOG_CONTAINER_UID" in
			-u\ *)
				container_uid_args="$RSYSLOG_CONTAINER_UID"
				container_uid_spec=${RSYSLOG_CONTAINER_UID#-u }
				;;
			-u*)
				container_uid_args="$RSYSLOG_CONTAINER_UID"
				container_uid_spec=${RSYSLOG_CONTAINER_UID#-u}
				;;
			*)
				container_uid_args="-u $RSYSLOG_CONTAINER_UID"
				container_uid_spec="$RSYSLOG_CONTAINER_UID"
				;;
		esac
	fi
else
	container_uid_spec="$(id -u):$(id -g)"
	container_uid_args="-u $container_uid_spec"
fi
passwd_group_mounts=
devcontainer_userdb_tmp=
devcontainer_userdb_cid=
cleanup_devcontainer_userdb() {
	if [ -n "$devcontainer_userdb_cid" ]; then
		docker rm "$devcontainer_userdb_cid" >/dev/null 2>&1 || true
	fi
	if [ -n "$devcontainer_userdb_tmp" ]; then
		rm -rf "$devcontainer_userdb_tmp"
	fi
}
case "$container_uid_spec" in
	[0-9]*:[0-9]*)
	container_uid="${container_uid_spec%%:*}"
	container_gid="${container_uid_spec##*:}"
	devcontainer_userdb_tmp="$(mktemp -d)"
	trap cleanup_devcontainer_userdb EXIT
	devcontainer_userdb_cid=$(docker create "$RSYSLOG_DEV_CONTAINER")
	docker cp "$devcontainer_userdb_cid:/etc/passwd" "$devcontainer_userdb_tmp/passwd"
	docker cp "$devcontainer_userdb_cid:/etc/group" "$devcontainer_userdb_tmp/group"
	docker cp "$devcontainer_userdb_cid:/etc/shadow" "$devcontainer_userdb_tmp/shadow"
	if ! awk -F: -v uid="$container_uid" '$3 == uid { found = 1 } END { exit found ? 0 : 1 }' \
		"$devcontainer_userdb_tmp/passwd"; then
		container_user="rsyslog-dev-$container_uid"
		shadow_last_change=$(($(date +%s) / 86400))
		printf 'rsyslog-dev-%s:x:%s:%s:rsyslog devcontainer user:/rsyslog:/bin/bash\n' \
			"$container_uid" "$container_uid" "$container_gid" >> "$devcontainer_userdb_tmp/passwd"
		printf '%s::%s:0:99999:7:::\n' "$container_user" "$shadow_last_change" >> \
			"$devcontainer_userdb_tmp/shadow"
	fi
	if ! awk -F: -v gid="$container_gid" '$3 == gid { found = 1 } END { exit found ? 0 : 1 }' \
		"$devcontainer_userdb_tmp/group"; then
		printf 'rsyslog-dev-%s:x:%s:\n' "$container_gid" "$container_gid" >> "$devcontainer_userdb_tmp/group"
	fi
	chmod 0644 "$devcontainer_userdb_tmp/passwd" "$devcontainer_userdb_tmp/group"
	chmod 0640 "$devcontainer_userdb_tmp/shadow"
	passwd_group_mounts="-v $devcontainer_userdb_tmp/passwd:/etc/passwd:ro"
	passwd_group_mounts="$passwd_group_mounts -v $devcontainer_userdb_tmp/group:/etc/group:ro"
	passwd_group_mounts="$passwd_group_mounts -v $devcontainer_userdb_tmp/shadow:/etc/shadow:ro"
	;;
esac
printf 'user ids: %s:%s\n' "$(id -u)" "$(id -g)"
printf 'container_uid: %s\n' "$container_uid_args"
printf 'container cmd: %s\n' "$*"
# DOCKER_RUN_EXTRA_OPTS and DOCKER_RUN_EXTRA_FLAGS intentionally support
# multiple docker-run arguments.
# shellcheck disable=SC2086
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
	-e RSYSLOG_TESTBENCH_EXTERNAL_VM_URL \
	-e CODECOV_commit_sha \
	-e CODECOV_repo_slug \
	-e VCS_SLUG \
	-e VERBOSE \
	--cap-add SYS_ADMIN \
	--cap-add SYS_PTRACE \
	$container_uid_args \
	$passwd_group_mounts \
	$DOCKER_RUN_EXTRA_FLAGS \
	-v "$RSYSLOG_HOME":/rsyslog "$RSYSLOG_DEV_CONTAINER" "$@"
