#!/bin/bash
# added 2026 for https://github.com/rsyslog/rsyslog/issues/7402, released under ASL 2.0
#
# Note: on buildbot VMs (where there is no environment cleanup), the
# kubernetes test server may be kept running if the script aborts or
# is aborted (buildbot master failure!) for some reason. As such we
# execute it under "timeout" control, which ensures it always is
# terminated. It's not a 100% great method, but hopefully does the
# trick. -- rgerhards, 2018-07-21
#
# Verify mmkubernetes reloads the bearer token from tokenfile and retries once
# on an HTTP 401. The module reads the ServiceAccount token from tokenfile once
# at worker start and caches it in the curl Authorization header. On a
# short-lived projected token that is rotated on disk, the cached copy goes
# stale and every lookup 401s. This test rotates the token file after the worker
# has cached the old value, then confirms a subsequent (cache-miss) lookup 401s,
# reloads the fresh token, retries, and succeeds - so the record is still
# enriched.
#export RSYSLOG_DEBUG="debug"
# USE_VALGRIND may be set by the -vg variant that sources this script
. ${srcdir:=.}/diag.sh init
check_command_available timeout
pwd=$( pwd )
k8s_srv_port_file="${RSYSLOG_DYNNAME}mmk8s-test-server.port"
generate_conf

# the token file the module reads and the server validates against; the worker
# caches its contents at startup and only re-reads it on reload
token_file=$RSYSLOG_DYNNAME.spool/sa-token
printf 'token-v1' > $token_file

testsrv=mmk8s-test-server
echo starting kubernetes \"emulator\"
# MMK8S_EXPECTED_TOKEN_FILE enables bearer-token validation against $token_file
# (the server returns 401 on mismatch)
MMK8S_EXPECTED_TOKEN_FILE=$pwd/$token_file \
	timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py 0 ${RSYSLOG_DYNNAME}${testsrv}.pid ${RSYSLOG_DYNNAME}${testsrv}.started ${k8s_srv_port_file} > ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log 2>&1 &
BGPROCESS=$!
wait_file_exists "$k8s_srv_port_file"
k8s_srv_port="$(cat "$k8s_srv_port_file")"
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started
echo background mmkubernetes_test_server.py process id is $BGPROCESS

add_conf '
global(workDirectory="'$RSYSLOG_DYNNAME.spool'")
module(load="../plugins/impstats/.libs/impstats" interval="1"
	   log.file="'"$RSYSLOG_DYNNAME.spool"'/mmkubernetes-stats.log" log.syslog="off" format="cee")
module(load="../plugins/imfile/.libs/imfile")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../contrib/mmkubernetes/.libs/mmkubernetes")

template(name="mmk8s_template" type="list") {
    property(name="$!all-json-plain")
    constant(value="\n")
}

input(type="imfile" file="'$RSYSLOG_DYNNAME.spool'/pod-*.log" tag="kubernetes" addmetadata="on")
action(type="mmjsonparse" cookie="")
action(type="mmkubernetes" busyretryinterval="1" tokenfile="'$pwd/$token_file'" tokenreloadinterval="0"
       kubernetesurl="http://localhost:'$k8s_srv_port'"
       filenamerules=["rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:.%.%container_hash:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log",
	                  "rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"]
)
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="mmk8s_template")
'

# use the mmkubernetes valgrind suppressions when run under the -vg variant;
# startup/wait_shutdown dispatch to the valgrind path automatically when
# USE_VALGRIND is set
if [ "$USE_VALGRIND" == "YES" ]; then
	export EXTRA_VALGRIND_SUPPRESSIONS="--suppressions=$srcdir/mmkubernetes.supp"
fi
startup

# record 1: the token on disk (v1) matches - the worker caches v1 and the lookup
# succeeds normally. Draining the queue guarantees the worker has started and
# cached v1 *before* we rotate the token below.
cat > ${RSYSLOG_DYNNAME}.spool/pod-name1_namespace-name1_container-name1-id1.log <<EOF
{"log":"{\"message\":\"HEAD1 / 200 1ms - 9.0B\"}\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z","testid":1}
EOF
wait_queueempty

# rotate the projected token on disk, as kubelet does at ~80% of its lifetime.
# The worker still holds the cached v1 in its curl header; the server now only
# accepts v2.
printf 'token-v2' > $token_file

# record 2: a new pod/namespace is a cache miss, so the worker queries the API
# with its stale cached v1 token -> 401 -> reload v2 from disk -> retry -> 200.
cat > ${RSYSLOG_DYNNAME}.spool/pod-name2_namespace-name2_container-name2-id2.log <<EOF
{"log":"{\"message\":\"HEAD2 / 200 1ms - 9.0B\"}\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z","testid":2}
EOF
wait_queueempty

shutdown_when_empty
wait_shutdown
kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

# both records must be enriched with kubernetes metadata from the API: pod_id is
# derived from the API response (the pod uid), not from the log filename, so its
# presence proves the lookup succeeded - record 1 under the original token,
# record 2 only after the token was reloaded on the 401
content_check 'pod-name1-id'
content_check 'pod-name2-id'
# the reload-and-retry path must have fired for record 2
content_check --regex 'mmkubernetes: got \[401\].*reloaded SA token and retrying once'
# and the retry must have recovered - no surfaced Unauthorized error
assert_content_missing 'mmkubernetes: Unauthorized'

exit_test
