#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export COOKIE=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 10 | head -n 1)

generate_conf
add_conf '
template(name="outfmt" type="string" string="image=%$!metadata!Image%\n")
module(load="../contrib/imdocker/.libs/imdocker")
if $!metadata!Names == "'$COOKIE'" then {
  action(type="omfile" template="outfmt"  file="'$RSYSLOG_OUT_LOG'")
}
'

# launch a docker runtime and generate an empty log
docker run \
   --name $COOKIE \
   alpine:latest \
   /bin/sh -c 'echo' > /dev/null

#export RS_REDIR=-d
startup

shutdown_when_empty
wait_shutdown

content_check "image=alpine:latest"

docker container rm $COOKIE
exit_test
