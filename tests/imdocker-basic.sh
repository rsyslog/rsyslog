#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
NUMMESSAGES=1000
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export COOKIE=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 10 | head -n 1)

generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imdocker/.libs/imdocker"
        ListContainersOptions="all=true"
        GetContainerLogOptions="timestamps=0&follow=1&stdout=1&stderr=0")
if $!metadata!Names == "'$COOKIE'" then {
  action(type="omfile" template="outfmt"  file="'$RSYSLOG_OUT_LOG'")
}
'

# launch a docker runtime to generate some logs.
docker run \
   --name $COOKIE \
   -e NUMMESSAGES=$NUMMESSAGES \
   alpine \
   /bin/sh -c 'for i in $(seq 0 $((NUMMESSAGES-1))); do echo "$i"; done' > /dev/null

#export RS_REDIR=-d
startup

shutdown_when_empty
wait_shutdown

echo "cookie: $COOKIE, file name: $RSYSLOG_OUT_LOG"
seq_check

docker container rm $COOKIE
exit_test
