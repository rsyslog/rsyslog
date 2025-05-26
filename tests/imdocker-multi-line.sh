#!/bin/bash
# This is part of the rsyslog testbench, licensed under ASL 2.0

# imdocker unit tests are enabled with --enable-imdocker-tests
. ${srcdir:=.}/diag.sh init
export NUMMESSAGES=999
export QUEUE_EMPTY_CHECK_FUNC=wait_file_lines
export COOKIE=$(tr -dc 'a-zA-Z0-9' < /dev/urandom | fold -w 10 | head -n 1)

generate_conf
add_conf '
template(name="outfmt" type="string" string="%$!metadata!Names% %msg%\n")

module(load="../contrib/imdocker/.libs/imdocker" PollingInterval="1"
        ListContainersOptions="all=true"
        GetContainerLogOptions="timestamps=0&follow=1&stdout=1&stderr=0")

if $!metadata!Names == "'$COOKIE'" then {
  action(type="omfile" template="outfmt"  file="'$RSYSLOG_OUT_LOG'")
}
'

# launch a docker runtime to generate some logs.
# These logs started after start-up should get from beginning
docker run \
  --name $COOKIE \
  -e num_items=$((NUMMESSAGES+1)) \
  -l imdocker.startregex=^multi-line: \
  alpine \
  /bin/sh -c \
  'for i in `seq 1 $num_items`; do printf "multi-line: $i\n line2....\n line3....\n"; done' > /dev/null

startup
shutdown_when_empty
wait_shutdown
docker container rm $COOKIE

content_check_with_count "$COOKIE multi-line:" $NUMMESSAGES 10
## check if all the data we expect to get in the file is there
for i in $(seq 1 $NUMMESSAGES); do
  grep "$COOKIE multi-line: $i#012 line2....#012 line3...." $RSYSLOG_OUT_LOG > /dev/null 2>&1
  if [ ! $? -eq 0 ]; then
    echo "ERROR: expecting the string $COOKIE multi-line: item '$i', it's not there"
    error_exit 1
  fi
done
exit_test
