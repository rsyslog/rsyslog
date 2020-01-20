#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0
# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
export COOKIE=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 10 | head -n 1)
SIZE=17000
generate_conf
add_conf '
template(name="outfmt" type="string" string="%msg%\n")
module(load="../contrib/imdocker/.libs/imdocker"
        ListContainersOptions="all=true"
        GetContainerLogOptions="timestamps=0&follow=1&stdout=1&stderr=0")

if $!metadata!Names == "'$COOKIE'" then {
  action(type="omfile" template="outfmt"  file="'$RSYSLOG_OUT_LOG'")
}

$MaxMessageSize 64k
'
# launch container with a long log line
docker run \
  --name $COOKIE \
  -e size=$SIZE \
  alpine /bin/sh -c 'echo "$(yes a | head -n $size | tr -d "\n")";' > /dev/null

startup
wait_file_lines "$RSYSLOG_OUT_LOG" 1
shutdown_when_empty
wait_shutdown

# check the log line length
echo "file name: $RSYSLOG_OUT_LOG"
count=$(grep "aaaaaaa" $RSYSLOG_OUT_LOG | tr -d "\n" | wc -c)

if [ "x$count" == "x$SIZE" ]; then
  echo "correct log line length: $count"
else
  echo "Incorrect log line length - found $count, expected: $SIZE"
  error_exit 1
fi

docker container rm $COOKIE
exit_test
