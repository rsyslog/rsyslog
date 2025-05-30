#!/bin/bash
# added 2018-04-06 by richm, released under ASL 2.0
#
# Note: on buildbot VMs (where there is no environment cleanup), the
# kubernetes test server may be kept running if the script aborts or
# is aborted (buildbot master failure!) for some reason. As such we
# execute it under "timeout" control, which ensure it always is
# terminated. It's not a 100% great method, but hopefully does the
# trick. -- rgerhards, 2018-07-21
. ${srcdir:=.}/diag.sh init
check_command_available timeout
pwd=$( pwd )
k8s_srv_port=$( get_free_port )
generate_conf
cachettl=10
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
action(type="mmkubernetes" token="dummy" kubernetesurl="http://localhost:'$k8s_srv_port'"
       cacheexpireinterval="1" cacheentryttl="'$cachettl'"
       filenamerules=["rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:.%.%container_hash:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log",
	                  "rule=:'$pwd/$RSYSLOG_DYNNAME.spool'/%pod_name:char-to:_%_%namespace_name:char-to:_%_%container_name_and_id:char-to:.%.log"]
)
action(type="omfile" file=`echo $RSYSLOG_OUT_LOG` template="mmk8s_template")
'

testsrv=mmk8s-test-server
echo starting kubernetes \"emulator\"
timeout 2m $PYTHON -u $srcdir/mmkubernetes_test_server.py $k8s_srv_port ${RSYSLOG_DYNNAME}${testsrv}.pid ${RSYSLOG_DYNNAME}${testsrv}.started > ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log 2>&1 &
BGPROCESS=$!
wait_process_startup ${RSYSLOG_DYNNAME}${testsrv} ${RSYSLOG_DYNNAME}${testsrv}.started
echo background mmkubernetes_test_server.py process id is $BGPROCESS

if [ "x${USE_VALGRIND:-NO}" == "xYES" ] ; then
	export EXTRA_VALGRIND_SUPPRESSIONS="--suppressions=$srcdir/mmkubernetes.supp"
	startup_vg
else
	startup
    if [ -n "${USE_GDB:-}" ] ; then
        echo attach gdb here
        sleep 54321 || :
    fi  
fi

# add 3 logs - then wait $cachettl - the first log should prime the cache - the next two should be pulled from the cache
cat > ${RSYSLOG_DYNNAME}.spool/pod-name.log <<EOF
{"message":"msg1","CONTAINER_NAME":"some-prefix_container-name1_pod-name1_namespace-name1_unused1_unused11","CONTAINER_ID_FULL":"id1","testid":1}
EOF
sleep 2
cat >> ${RSYSLOG_DYNNAME}.spool/pod-name.log <<EOF
{"message":"msg2","CONTAINER_NAME":"some-prefix_container-name1_pod-name1_namespace-name1_unused1_unused11","CONTAINER_ID_FULL":"id1","testid":2}
EOF
sleep 2
cat >> ${RSYSLOG_DYNNAME}.spool/pod-name.log <<EOF
{"message":"msg3","CONTAINER_NAME":"some-prefix_container-name1_pod-name1_namespace-name1_unused1_unused11","CONTAINER_ID_FULL":"id1","testid":3}
EOF
sleep $cachettl
# we should see
# - namespacecachenumentries and podcachenumentries go from 0 to 1 then back to 0
# - namespacecachehits and podcachehits go from 0 to 2
# - namespacecachemisses and podcachemisses go from 0 to 1
# add another record - should not be cached - should have expired
cat >> ${RSYSLOG_DYNNAME}.spool/pod-name.log <<EOF
{"message":"msg4","CONTAINER_NAME":"some-prefix_container-name1_pod-name1_namespace-name1_unused1_unused11","CONTAINER_ID_FULL":"id1","testid":4}
EOF
# we should see
# - namespacecachenumentries and podcachenumentries back to 1
# - namespacecachehits and podcachehits did not increase
# - namespacecachemisses and podcachemisses increases

# wait for the first batch of tests to complete
wait_queueempty

shutdown_when_empty
if [ "x${USE_VALGRIND:-NO}" == "xYES" ] ; then
	wait_shutdown_vg
	check_exit_vg
else
	wait_shutdown
fi
kill $BGPROCESS
wait_pid_termination ${RSYSLOG_DYNNAME}${testsrv}.pid

rc=0
# for each record in mmkubernetes-cache-expire.out.json, see if the matching
# record is found in $RSYSLOG_OUT_LOG
$PYTHON -c 'import sys,json
k8s_srv_port = sys.argv[3]
expected = {}
for hsh in json.load(open(sys.argv[1])):
	if "testid" in hsh:
		if "kubernetes" in hsh and "master_url" in hsh["kubernetes"]:
			hsh["kubernetes"]["master_url"] = hsh["kubernetes"]["master_url"].format(k8s_srv_port=k8s_srv_port)
		expected[hsh["testid"]] = hsh
rc = 0
actual = {}
for line in open(sys.argv[2]):
	hsh = json.loads(line)
	if "testid" in hsh:
		actual[hsh["testid"]] = hsh
for testid,hsh in expected.items():
	if not testid in actual:
		print("Error: record for testid {0} not found in output".format(testid))
		rc = 1
	else:
		for kk,vv in hsh.items():
			if not kk in actual[testid]:
				print("Error: key {0} in record for testid {1} not found in output".format(kk, testid))
				rc = 1
			elif not vv == actual[testid][kk]:
				print("Error: value {0} for key {1} in record for testid {2} does not match the expected value {3}".format(str(actual[testid][kk]), kk, testid, str(vv)))
				rc = 1
sys.exit(rc)
' $srcdir/mmkubernetes-cache-expire.out.expected $RSYSLOG_OUT_LOG $k8s_srv_port || rc=$?

if [ -f ${RSYSLOG_DYNNAME}.spool/mmkubernetes-stats.log ] ; then
	$PYTHON <${RSYSLOG_DYNNAME}.spool/mmkubernetes-stats.log  -c '
import sys,json
# key is recordseen, value is hash of stats for that record
expectedvalues = {
	1: {"namespacecachenumentries": 1, "podcachenumentries": 1, "namespacecachehits": 0,
	    "podcachehits": 0, "namespacecachemisses": 1, "podcachemisses": 1},
	2: {"namespacecachenumentries": 1, "podcachenumentries": 1, "namespacecachehits": 0,
	    "podcachehits": 1, "namespacecachemisses": 1, "podcachemisses": 1},
	3: {"namespacecachenumentries": 1, "podcachenumentries": 1, "namespacecachehits": 0,
	    "podcachehits": 2, "namespacecachemisses": 1, "podcachemisses": 1},
    4: {"namespacecachenumentries": 1, "podcachenumentries": 1, "namespacecachehits": 0,
	   "podcachehits": 2, "namespacecachemisses": 2, "podcachemisses": 2},
}
for line in sys.stdin:
	jstart = line.find("{")
	if jstart >= 0:
		hsh = json.loads(line[jstart:])
		if hsh.get("origin") == "mmkubernetes" and hsh["recordseen"] in expectedvalues:
			expected = expectedvalues[hsh["recordseen"]]
			for key in expected:
				if not expected[key] == hsh.get(key):
					print("Error: expected value [%s] not equal to actual value [%s] in record [%d] for stat [%s]".format(
					str(expected[key]), str(hsh[key]), hsh["recordseen"], key))
' || { rc=$?; echo error: expected stats not found in ${RSYSLOG_DYNNAME}.spool/mmkubernetes-stats.log; }
else
	echo error: stats file ${RSYSLOG_DYNNAME}.spool/mmkubernetes-stats.log not found
	rc=1
fi

if [ ${rc:-0} -ne 0 ]; then
	echo
	echo "FAIL: expected data not found.  $RSYSLOG_OUT_LOG is:"
	cat ${RSYSLOG_DYNNAME}.spool/mmk8s_srv.log
	cat $RSYSLOG_OUT_LOG
	cat ${RSYSLOG_DYNNAME}.spool/mmkubernetes-stats.log
	error_exit 1
fi

exit_test
