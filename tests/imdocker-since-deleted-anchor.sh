#!/bin/bash
# Regression test for imdocker container discovery after Docker rejects the
# internal "since=<last container id>" anchor. The oracle is that rsyslog first
# reads a short-lived container, then still discovers and reads a second
# container after the first one has been removed.
# This is part of the rsyslog testbench, licensed under ASL 2.0
. ${srcdir:=.}/diag.sh init

command -v docker >/dev/null 2>&1 || skip_test
docker info >/dev/null 2>&1 || skip_test

COOKIE=$(LC_ALL=C tr -dc 'a-zA-Z0-9' < /dev/urandom | head -c 10)
OLD_CONTAINER="rsyslog-${COOKIE}-old"
NEW_CONTAINER="rsyslog-${COOKIE}-new"

cleanup_docker() {
	docker rm -f "$OLD_CONTAINER" "$NEW_CONTAINER" >/dev/null 2>&1 || true
}
trap cleanup_docker EXIT
cleanup_docker

generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!metadata!Names% %msg%\n")
module(load="../contrib/imdocker/.libs/imdocker"
        PollingInterval="1"
        ListContainersOptions="all=true"
        GetContainerLogOptions="timestamps=0&follow=1&stdout=1&stderr=0&tail=all")
if $!metadata!Names == "'$OLD_CONTAINER'" or $!metadata!Names == "'$NEW_CONTAINER'" then {
  action(type="omfile" template="outfmt" file="'$RSYSLOG_OUT_LOG'")
}
'

startup

docker run --name "$OLD_CONTAINER" alpine /bin/sh -c 'echo old-anchor'
wait_content "old-anchor"
docker rm "$OLD_CONTAINER"

docker run --name "$NEW_CONTAINER" alpine /bin/sh -c 'echo new-after-deleted-anchor'
wait_content "new-after-deleted-anchor"

shutdown_when_empty
wait_shutdown

exit_test
