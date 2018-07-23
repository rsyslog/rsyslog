#!/bin/bash
# added 2018-04-06 by richm, released under ASL 2.0
#
# Note: on buidbot VMs (where there is no environment cleanup), the
# kubernetes test server may be kept running if the script aborts or
# is aborted (buildbot master failure!) for some reason. As such we
# execute it under "timeout" control, which ensure it always is
# terminated. It's not a 100% great method, but hopefully does the
# trick. -- rgerhards, 2018-07-21
#export RSYSLOG_DEBUG="debug"
. $srcdir/diag.sh init
. $srcdir/diag.sh check-command-available timeout

testsrv=mmk8s-test-server
timeout 3m python ./mmkubernetes_test_server.py 18443 rsyslog${testsrv}.pid rsyslogd${testsrv}.started > mmk8s_srv.log 2>&1 &
BGPROCESS=$!
. $srcdir/diag.sh wait-startup $testsrv
echo background mmkubernetes_test_server.py process id is $BGPROCESS

pwd=$( pwd )
generate_conf
add_conf '
module(load="../plugins/imfile/.libs/imfile")
module(load="../plugins/mmjsonparse/.libs/mmjsonparse")
module(load="../contrib/mmkubernetes/.libs/mmkubernetes" token="dummy" kubernetesurl="http://localhost:18443"
       filenamerules=["rule=:'$pwd'/%pod_name:char-to:.%.%container_hash:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log",
	                  "rule=:'$pwd'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"]
)

template(name="mmk8s_template" type="list") {
    property(name="$!all-json-plain")
    constant(value="\n")
}

input(type="imfile" file="'$pwd'/pod-*.log" tag="kubernetes" addmetadata="on")
action(type="mmjsonparse" cookie="")
action(type="mmkubernetes")
action(type="omfile" file="rsyslog.out.log" template="mmk8s_template")
'
cat > pod-name1_namespace-name1_container-name1-id1.log <<EOF
{"log":"{\"type\":\"response\",\"@timestamp\":\"2018-04-06T17:26:34Z\",\"tags\":[],\"pid\":75,\"method\":\"head\",\"statusCode\":200,\"req\":{\"url\":\"/\",\"method\":\"head\",\"headers\":{\"user-agent\":\"curl/7.29.0\",\"host\":\"localhost:5601\",\"accept\":\"*/*\"},\"remoteAddress\":\"127.0.0.1\",\"userAgent\":\"127.0.0.1\"},\"res\":{\"statusCode\":200,\"responseTime\":1,\"contentLength\":9},\"message\":\"HEAD1 / 200 1ms - 9.0B\"}\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z"}
EOF
cat > pod-name2.container-hash2_namespace-name2_container-name2-id2.log <<EOF
{"log":"{\"type\":\"response\",\"@timestamp\":\"2018-04-06T17:26:34Z\",\"tags\":[],\"pid\":75,\"method\":\"head\",\"statusCode\":200,\"req\":{\"url\":\"/\",\"method\":\"head\",\"headers\":{\"user-agent\":\"curl/7.29.0\",\"host\":\"localhost:5601\",\"accept\":\"*/*\"},\"remoteAddress\":\"127.0.0.1\",\"userAgent\":\"127.0.0.1\"},\"res\":{\"statusCode\":200,\"responseTime\":1,\"contentLength\":9},\"message\":\"HEAD2 / 200 1ms - 9.0B\"}\n","stream":"stdout","time":"2018-04-06T17:26:34.492083106Z"}
EOF
cat > pod-name3.log <<EOF
{"message":"a message from container 3","CONTAINER_NAME":"some-prefix_container-name3.container-hash3_pod-name3_namespace-name3_unused3_unused33","CONTAINER_ID_FULL":"id3"}
EOF
cat > pod-name4.log <<EOF
{"message":"a message from container 4","CONTAINER_NAME":"some-prefix_container-name4_pod-name4_namespace-name4_unused4_unused44","CONTAINER_ID_FULL":"id4"}
EOF
rm -f imfile-state\:*
startup_vg_noleak
sleep 10 || :
shutdown_when_empty
wait_shutdown_vg
. $srcdir/diag.sh check-exit-vg

kill $BGPROCESS
. $srcdir/diag.sh wait-pid-termination rsyslog${testsrv}.pid
cat mmk8s_srv.log

# for each record in mmkubernetes-basic.out.json, see if the matching
# record is found in rsyslog.out.log
python -c 'import sys,json
expected = {}
for hsh in json.load(open(sys.argv[1])):
	if "kubernetes" in hsh and "pod_name" in hsh["kubernetes"]:
		expected[hsh["kubernetes"]["pod_name"]] = hsh
rc = 0
actual = {}
for line in open(sys.argv[2]):
	hsh = json.loads(line)
	if "kubernetes" in hsh and "pod_name" in hsh["kubernetes"]:
		actual[hsh["kubernetes"]["pod_name"]] = hsh
for pod,hsh in expected.items():
	if not pod in actual:
		print("Error: record for pod {0} not found in output".format(pod))
		rc = 1
	else:
		for kk,vv in hsh.items():
			if not kk in actual[pod]:
				print("Error: key {0} in record for pod {1} not found in output".format(kk, pod))
				rc = 1
			elif not vv == actual[pod][kk]:
				print("Error: value {0} for key {1} in record for pod {2} does not match the expected value {3}".format(actual[pod][kk], kk, pod, vv))
				rc = 1
sys.exit(rc)
' mmkubernetes-basic.out.json rsyslog.out.log
if [ $? -ne 0 ]; then
	echo
	echo "FAIL: expected data not found. rsyslog.out.log is:"
	cat rsyslog.out.log
	error_exit 1
fi

exit_test
