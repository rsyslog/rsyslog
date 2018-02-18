#!/bin/bash
# 
# this shell script provides commands to the common diag system. It enables
# test scripts to wait for certain conditions and initiate certain actions.
# needs support in config file.
# NOTE: this file should be included with ". diag.sh", as it otherwise is
# not always able to convey back states to the upper-level test driver
# begun 2009-05-27 by rgerhards
# This file is part of the rsyslog project, released under GPLv3
#
# This file can be customized to environment specifics via environment
# variables:
# RS_SORTCMD    Sort command to use (must support -g option). If unset,
#		"sort" is used. E.g. Solaris needs "gsort"
# RS_CMPCMD     cmp command to use. If unset, "cmd" is used.
#               E.g. Solaris needs "gcmp"
# RS_HEADCMD    head command to use. If unset, "head" is used.
#               E.g. Solaris needs "ghead"
#

# environment variables:
# USE_AUTO_DEBUG "on" --> enables automatic debugging, anything else
#                turns it off

# diag system internal environment variables
# these variables are for use by test scripts - they CANNOT be
# overriden by the user
# TCPFLOOD_EXTRA_OPTS   enables to set extra options for tcpflood, usually
#                       used in tests that have a common driver where it
#                       is too hard to set these options otherwise

#valgrind="valgrind --malloc-fill=ff --free-fill=fe --log-fd=1"

# **** use the line below for very hard to find leaks! *****
#valgrind="valgrind --leak-check=full --show-leak-kinds=all --malloc-fill=ff --free-fill=fe --log-fd=1"

#valgrind="valgrind --tool=drd --log-fd=1"
#valgrind="valgrind --tool=helgrind --log-fd=1 --suppressions=linux_localtime_r.supp --gen-suppressions=all"
#valgrind="valgrind --tool=exp-ptrcheck --log-fd=1"
#set -o xtrace
#export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
#export RSYSLOG_DEBUGLOG="log"
TB_TIMEOUT_STARTSTOP=400 # timeout for start/stop rsyslogd in tenths (!) of a second 400 => 40 sec
# note that 40sec for the startup should be sufficient even on very slow machines. we changed this from 2min on 2017-12-12

function rsyslog_testbench_test_url_access() {
    local missing_requirements=
    if ! hash curl 2>/dev/null ; then
        missing_requirements="'curl' is missing in PATH; Make sure you have cURL installed! Skipping test ..."
    fi

    if [ -n "${missing_requirements}" ]; then
        echo ${missing_requirements}
        exit 77
    fi

    local http_endpoint="$1"
    if ! curl --fail --max-time 30 "${http_endpoint}" 1>/dev/null 2>&1; then
        echo "HTTP endpont '${http_endpoint}' isn't reachable. Skipping test ..."
        exit 77
    else
        echo "HTTP endpoint '${http_endoint}' is reachable! Starting test ..."
    fi
}

#START: ext kafka config
dep_cache_dir=$(readlink -f $srcdir/.dep_cache)
dep_zk_url=http://www-us.apache.org/dist/zookeeper/zookeeper-3.4.10/zookeeper-3.4.10.tar.gz
dep_zk_cached_file=$dep_cache_dir/zookeeper-3.4.10.tar.gz
dep_kafka_url=http://www-us.apache.org/dist/kafka/0.10.2.1/kafka_2.12-0.10.2.1.tgz
if [ -z "$ES_DOWNLOAD" ]; then
	dep_es_url=https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-5.6.3.tar.gz
	dep_es_cached_file=$dep_cache_dir/elasticsearch-5.6.3.tar.gz
else
	dep_es_url="https://artifacts.elastic.co/downloads/elasticsearch/$ES_DOWNLOAD"
	dep_es_cached_file="$dep_cache_dir/$ES_DOWNLOAD"
fi
dep_kafka_cached_file=$dep_cache_dir/kafka_2.12-0.10.2.1.tgz
dep_kafka_dir_xform_pattern='s#^[^/]\+#kafka#g'
dep_zk_dir_xform_pattern='s#^[^/]\+#zk#g'
dep_es_dir_xform_pattern='s#^[^/]\+#es#g'
dep_kafka_log_dump=$(readlink -f $srcdir/rsyslog.out.kafka.log)

#	TODO Make dynamic work dir for multiple instances
dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
#dep_kafka_work_dir=$dep_work_dir/kafka
#dep_zk_work_dir=$dep_work_dir/zk

#END: ext kafka config

case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		# for (solaris) load debugging, uncomment next 2 lines:
		#export LD_DEBUG=all
		#ldd ../tools/rsyslogd

		# environment debug
		#find / -name "librelp.so*"
		#ps -ef |grep syslog
		#netstat -a | grep LISTEN

		# cleanup of hanging instances from previous runs
		# practice has shown this is pretty useful!
		for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		do
			echo "ERROR: left-over previous instance $pid, killing it"
			ps -fp $pid
			kill -9 $pid
		done
		# end cleanup

		if [ -z $RS_SORTCMD ]; then
			RS_SORTCMD=sort
		fi  
		if [ -z $RS_CMPCMD ]; then
			RS_CMPCMD=cmp
		fi
		if [ -z $RS_HEADCMD ]; then
			RS_HEADCMD=head
		fi
		ulimit -c unlimited  &> /dev/null # at least try to get core dumps
		echo "------------------------------------------------------------"
		echo "Test: $0"
		echo "------------------------------------------------------------"
		# we assume TZ is set, else most test will fail. So let's ensure
		# this really is the case
		if [ -z $TZ ]; then
			echo "testbench: TZ env var not set, setting it to UTC"
			export TZ=UTC
		fi

		cp $srcdir/testsuites/diag-common.conf diag-common.conf
		cp $srcdir/testsuites/diag-common2.conf diag-common2.conf
		rm -f rsyslogd.started work-*.conf rsyslog.random.data
		rm -f rsyslogd2.started work-*.conf
		rm -f log log* # RSyslog debug output 
		rm -f work rsyslog.out.log rsyslog2.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.out.*.log work-presort rsyslog.pipe
		rm -f -r rsyslog.input.*
		rm -f rsyslog.input rsyslog.empty rsyslog.input.* imfile-state* omkafka-failed.data
		rm -f testconf.conf HOSTNAME
		rm -f rsyslog.errorfile tmp.qi
		rm -f core.* vgcore.* core*
		# Note: rsyslog.action.*.include must NOT be deleted, as it
		# is used to setup some parameters BEFORE calling init. This
		# happens in chained test scripts. Delete on exit is fine,
		# though.
		mkdir test-spool
		# note: TCPFLOOD_EXTRA_OPTS MUST NOT be unset in init, because
		# some tests need to set it BEFORE calling init to accomodate
		# their generic test drivers.
		if [ "$TCPFLOOD_EXTRA_OPTS" != '' ] ; then
		        echo TCPFLOOD_EXTRA_OPTS set: $TCPFLOOD_EXTRA_OPTS
                fi
		if [ "$USE_AUTO_DEBUG" != 'on' ] ; then
			rm -f IN_AUTO_DEBUG
                fi
		if [ -e IN_AUTO_DEBUG ]; then
			export valgrind="valgrind --malloc-fill=ff --free-fill=fe --log-fd=1"
		fi
		export RSYSLOG_DFLT_LOG_INTERNAL=1 # testbench needs internal messages logged internally!
		;;
   'exit')	# cleanup
		# detect any left-over hanging instance
		nhanging=0
		for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		do
			echo "ERROR: left-over instance $pid, killing it"
			ps -fp $pid
			kill -9 $pid
			let "nhanging++"
		done
		if test $nhanging -ne 0
		then
		   echo "ABORT! hanging instances left at exit"
		   . $srcdir/diag.sh error-exit 1
		fi
		# now real cleanup
		rm -f rsyslogd.started work-*.conf diag-common.conf
   		rm -f rsyslogd2.started diag-common2.conf rsyslog.action.*.include
		rm -f work rsyslog.out.log rsyslog2.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.out.*.log rsyslog.random.data work-presort rsyslog.pipe
		rm -f -r rsyslog.input.*
		rm -f rsyslog.input rsyslog.conf.tlscert stat-file1 rsyslog.empty rsyslog.input.* imfile-state*
		rm -f testconf.conf
		rm -f rsyslog.errorfile tmp.qi
		rm -f HOSTNAME imfile-state:.-rsyslog.input
		unset TCPFLOOD_EXTRA_OPTS
		echo  -------------------------------------------------------------------------------
		;;
   'check-url-access')   # check if we can access the URL - will exit 77 when not OK
		rsyslog_testbench_test_url_access $2
		;;
   'check-command-available')   # check if command $2 is available - will exit 77 when not OK
		command -v $2
		if [ $? -ne 0 ] ; then
			echo Testbench requires unavailable command: $2
			echo skipping this test
			exit 77
		fi
		;;
   'es-init')   # initialize local Elasticsearch *testbench* instance for the next
                # test. NOTE: do NOT put anything useful on that instance!
		curl -XDELETE localhost:9200/rsyslog_testbench
		;;
   'es-getdata') # read data from ES to a local file so that we can process
		if [ "x$3" == "x" ]; then
			es_get_port=9200
		else
			es_get_port=$3
		fi

   		# it with out regular tooling.
		# Note: param 2 MUST be number of records to read (ES does
		# not return the full set unless you tell it explicitely).
		curl localhost:$es_get_port/rsyslog_testbench/_search?size=$2 > work
		python $srcdir/es_response_get_msgnum.py > rsyslog.out.log
		;;
   'getpid')
		pid=$(cat rsyslog$2.pid)
		;;
   'startup')   # start rsyslogd with default params. $2 is the config file name to use
   		# returns only after successful startup, $3 is the instance (blank or 2!)
		if [ "x$2" == "x" ]; then
		    CONF_FILE="testconf.conf"
		    echo $CONF_FILE is:
		    cat -n $CONF_FILE
		else
		    CONF_FILE="$srcdir/testsuites/$2"
		fi
		if [ ! -f $CONF_FILE ]; then
		    echo "ERROR: config file '$CONF_FILE' not found!"
		    exit 1
		fi
		LD_PRELOAD=$RSYSLOG_PRELOAD $valgrind ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
		. $srcdir/diag.sh wait-startup $3
		;;
   'startup-silent')   # start rsyslogd with default params. $2 is the config file name to use
   		# returns only after successful startup, $3 is the instance (blank or 2!)
		if [ ! -f $srcdir/testsuites/$2 ]; then
		    echo "ERROR: config file '$srcdir/testsuites/$2' not found!"
		    exit 1
		fi
		$valgrind ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 2>/dev/null &
		. $srcdir/diag.sh wait-startup $3
		;;
   'startup-vg-waitpid-only') # same as startup-vg, BUT we do NOT wait on the startup message!
		if [ "x$RS_TESTBENCH_LEAK_CHECK" == "x" ]; then
		    RS_TESTBENCH_LEAK_CHECK=full
		fi
		if [ "x$2" == "x" ]; then
		    CONF_FILE="testconf.conf"
		    echo $CONF_FILE is:
		    cat -n $CONF_FILE
		else
		    CONF_FILE="$srcdir/testsuites/$2"
		fi
		if [ ! -f $CONF_FILE ]; then
		    echo "ERROR: config file '$CONF_FILE' not found!"
		    exit 1
		fi
		LD_PRELOAD=$RSYSLOG_PRELOAD valgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --gen-suppressions=all --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=$RS_TESTBENCH_LEAK_CHECK ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
		. $srcdir/diag.sh wait-startup-pid $3
		;;
   'startup-vg') # start rsyslogd with default params under valgrind control. $2 is the config file name to use
		# returns only after successful startup, $3 is the instance (blank or 2!)
		. $srcdir/diag.sh startup-vg-waitpid-only $2 $3
		. $srcdir/diag.sh wait-startup $3
		echo startup-vg still running
		;;
   'startup-vg-noleak') # same as startup-vg, except that --leak-check is set to "none". This
   		# is meant to be used in cases where we have to deal with libraries (and such
		# that) we don't can influence and where we cannot provide suppressions as
		# they are platform-dependent. In that case, we can't test for leak checks
		# (obviously), but we can check for access violations, what still is useful.
		RS_TESTBENCH_LEAK_CHECK=no
		. $srcdir/diag.sh startup-vg-waitpid-only $2 $3
		. $srcdir/diag.sh wait-startup $3
		echo startup-vg still running
		;;
	 'msleep')
   	$srcdir/msleep $2
		;;

   'startup-vgthread-waitpid-only') # same as startup-vgthread, BUT we do NOT wait on the startup message!
		if [ "x$2" == "x" ]; then
		    CONF_FILE="testconf.conf"
		    echo $CONF_FILE is:
		    cat -n $CONF_FILE
		else
		    CONF_FILE="$srcdir/testsuites/$2"
		fi
		if [ ! -f $CONF_FILE ]; then
		    echo "ERROR: config file '$CONF_FILE' not found!"
		    exit 1
		fi
		valgrind --tool=helgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --suppressions=linux_localtime_r.supp --gen-suppressions=all ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
		. $srcdir/diag.sh wait-startup-pid $3
		;;
   'startup-vgthread') # start rsyslogd with default params under valgrind thread debugger control.
   		# $2 is the config file name to use
		# returns only after successful startup, $3 is the instance (blank or 2!)
		. $srcdir/diag.sh startup-vgthread-waitpid-only $2 $3
		. $srcdir/diag.sh wait-startup $3
		;;
   'wait-startup-pid') # wait for rsyslogd startup, PID only ($2 is the instance)
		i=0
		while test ! -f rsyslog$2.pid; do
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on startup (pid file)"
			   . $srcdir/diag.sh error-exit 1
			fi
		done
		echo "rsyslogd$2 started, start msg not yet seen, pid " `cat rsyslog$2.pid`
		;;
   'wait-startup') # wait for rsyslogd startup ($2 is the instance)
		. $srcdir/diag.sh wait-startup-pid $2
		i=0
		while test ! -f rsyslogd$2.started; do
			./msleep 100 # wait 100 milliseconds
			ps -p `cat rsyslog$2.pid` &> /dev/null
			if [ $? -ne 0 ]
			then
			   echo "ABORT! rsyslog pid no longer active during startup!"
			   . $srcdir/diag.sh error-exit 1 stacktrace
			fi
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on startup ('started' file)"
			   . $srcdir/diag.sh error-exit 1
			fi
		done
		echo "rsyslogd$2 startup msg seen, pid " `cat rsyslog$2.pid`
		;;
   'wait-pid-termination')  # wait for the pid in pid file $2 to terminate, abort on timeout
		i=0
		out_pid=`cat $2`
		if [[ "x$out_pid" == "x" ]]
		then
			terminated=1
		else
			terminated=0
		fi
		while [[ $terminated -eq 0 ]]; do
			ps -p $out_pid &> /dev/null
			if [[ $? != 0 ]]
			then
				terminated=1
			fi
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on shutdown"
			   echo "on PID file $2"
			   echo "Instance is possibly still running and may need"
			   echo "manual cleanup."
			   exit 1
			fi
		done
		unset terminated
		unset out_pid
		;;
   'wait-shutdown')  # actually, we wait for rsyslog.pid to be deleted. $2 is the
   		# instance
		i=0
		out_pid=`cat rsyslog$2.pid.save`
		if [[ "x$out_pid" == "x" ]]
		then
			terminated=1
		else
			terminated=0
		fi
		while [[ $terminated -eq 0 ]]; do
			ps -p $out_pid &> /dev/null
			if [[ $? != 0 ]]
			then
				terminated=1
			fi
			./msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on shutdown"
			   echo "Instance is possibly still running and may need"
			   echo "manual cleanup."
			   exit 1
			fi
		done
		unset terminated
		unset out_pid
		if [ -e core.* ]
		then
		   echo "ABORT! core file exists"
		   . $srcdir/diag.sh error-exit  1
		fi
		;;
   'wait-shutdown-vg')  # actually, we wait for rsyslog.pid to be deleted. $2 is the
   		# instance
		wait `cat rsyslog$2.pid`
		export RSYSLOGD_EXIT=$?
		echo rsyslogd run exited with $RSYSLOGD_EXIT
		if [ -e vgcore.* ]
		then
		   echo "ABORT! core file exists"
		   . $srcdir/diag.sh error-exit 1
		fi
		;;
   'check-exit-vg') # wait for main message queue to be empty. $2 is the instance.
		if [ "$RSYSLOGD_EXIT" -eq "10" ]
		then
			echo "valgrind run FAILED with exceptions - terminating"
			exit 1
		fi
		;;
   'get-mainqueuesize') # show the current main queue size
		if [ "$2" == "2" ]
		then
			echo getmainmsgqueuesize | ./diagtalker -p13501 || . $srcdir/diag.sh error-exit  $?
		else
			echo getmainmsgqueuesize | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		fi
		;;
   'wait-queueempty') # wait for main message queue to be empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo WaitMainQueueEmpty | ./diagtalker -p13501 || . $srcdir/diag.sh error-exit  $?
		else
			echo WaitMainQueueEmpty | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		fi
		;;
   'await-lookup-table-reload') # wait for all pending lookup table reloads to complete $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo AwaitLookupTableReload | ./diagtalker -p13501 || . $srcdir/diag.sh error-exit  $?
		else
			echo AwaitLookupTableReload | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		fi
		;;
   'issue-HUP') # shut rsyslogd down when main queue is empty. $2 is the instance.
		kill -HUP `cat rsyslog$2.pid`
		./msleep 1000
		;;
   'shutdown-when-empty') # shut rsyslogd down when main queue is empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
		   echo Shutting down instance 2
		fi
		. $srcdir/diag.sh wait-queueempty $2
		cp rsyslog$2.pid rsyslog$2.pid.save
		./msleep 1000 # wait a bit (think about slow testbench machines!)
		kill `cat rsyslog$2.pid`
		# note: we do not wait for the actual termination!
		;;
   'shutdown-immediate') # shut rsyslogd down without emptying the queue. $2 is the instance.
		cp rsyslog$2.pid rsyslog$2.pid.save
		kill `cat rsyslog$2.pid`
		# note: we do not wait for the actual termination!
		;;
   'kill-immediate') # kill rsyslog unconditionally
		kill -9 `cat rsyslog.pid`
		# note: we do not wait for the actual termination!
		;;
   'tcpflood') # do a tcpflood run and check if it worked params are passed to tcpflood
		shift 1
		eval ./tcpflood "$@" $TCPFLOOD_EXTRA_OPTS
		if [ "$?" -ne "0" ]; then
		  echo "error during tcpflood! see rsyslog.out.log.save for what was written"
		  cp rsyslog.out.log rsyslog.out.log.save
		  . $srcdir/diag.sh error-exit 1 stacktrace
		fi
		;;
   'injectmsg') # inject messages via our inject interface (imdiag)
		echo injecting $3 messages
		echo injectmsg $2 $3 $4 $5 | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		# TODO: some return state checking? (does it really make sense here?)
		;;
    'injectmsg-litteral') # inject litteral-payload  via our inject interface (imdiag)
		echo injecting msg payload from: $2
    cat $2 | sed -e 's/^/injectmsg litteral /g' | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		# TODO: some return state checking? (does it really make sense here?)
		;;
   'check-mainq-spool') # check if mainqueue spool files exist, if not abort (we just check .qi).
		echo There must exist some files now:
		ls -l test-spool
		echo .qi file:
		cat test-spool/mainq.qi
		if test ! -f test-spool/mainq.qi; then
		  echo "error: mainq.qi does not exist where expected to do so!"
		  . $srcdir/diag.sh error-exit 1
		fi
		;;
   'presort')	# sort the output file just like we normally do it, but do not call
		# seqchk. This is needed for some operations where we need the sort
		# result for some preprocessing. Note that a later seqchk will sort
		# again, but that's not an issue.
		rm -f work
		$RS_SORTCMD -g < rsyslog.out.log > work
		;;
   'assert-equal')
		if [ "x$4" == "x" ]; then
			tolerance=0
		else
			tolerance=$4
		fi
		result=$(echo $2 $3 $tolerance | awk 'function abs(v) { return v > 0 ? v : -v } { print (abs($1 - $2) <= $3) ? "PASSED" : "FAILED" }')
		if [ $result != 'PASSED' ]; then
				echo "Value '$2' != '$3' (within tolerance of $tolerance)"
		  . $srcdir/diag.sh error-exit 1
		fi
		;;
   'ensure-no-process-exists')
    ps -ef | grep -v grep | grep -qF "$2"
    if [ "x$?" == "x0" ]; then
      echo "assertion failed: process with name-fragment matching '$2' found"
		  . $srcdir/diag.sh error-exit 1
    fi
    ;;
   'grep-check') # grep for "$EXPECTED" present in rsyslog.log - env var must be set
		 # before this method is called
		grep "$EXPECTED" rsyslog.out.log > /dev/null
		if [ $? -eq 1 ]; then
		  echo "GREP FAIL: rsyslog.out.log content:"
		  cat rsyslog.out.log
		  echo "GREP FAIL: expected text not found:"
		  echo "$EXPECTED"
		. $srcdir/diag.sh error-exit 1
		fi;
		;;
   'seq-check') # do the usual sequence check to see if everything was properly received. $2 is the instance.
		rm -f work
		cp rsyslog.out.log work-presort
		$RS_SORTCMD -g < rsyslog.out.log > work
		# $4... are just to have the abilit to pass in more options...
		# add -v to chkseq if you need more verbose output
		./chkseq -fwork -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  . $srcdir/diag.sh error-exit 1 
		fi
		;;
   'seq-check2') # do the usual sequence check to see if everything was properly received. This is
   		# a duplicateof seq-check, but we could not change its calling conventions without
		# breaking a lot of exitings test cases, so we preferred to duplicate the code here.
		rm -f work2
		$RS_SORTCMD -g < rsyslog2.out.log > work2
		# $4... are just to have the abilit to pass in more options...
		# add -v to chkseq if you need more verbose output
		./chkseq -fwork2 -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  . $srcdir/diag.sh error-exit 1
		fi
		rm -f work2
		;;
   'content-cmp')
		echo "$2" | cmp - rsyslog.out.log
		if [ "$?" -ne "0" ]; then
		    echo content-cmp failed
		    echo EXPECTED:
		    echo $2
		    echo ACTUAL:
		    cat rsyslog.out.log
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'content-check')
		cat rsyslog.out.log | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed to find "'$2'", content is
		    cat rsyslog.out.log
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'content-check-with-count') 
		# content check variables for Timeout
		if [ "x$4" == "x" ]; then
			timeoutend=1
		else
			timeoutend=$4
		fi
		timecounter=0

		while [  $timecounter -lt $timeoutend ]; do
#			echo content-check-with-count loop $timecounter
			let timecounter=timecounter+1

			count=$(cat rsyslog.out.log | grep -F "$2" | wc -l)

			if [ "x$count" == "x$3" ]; then
				echo content-check-with-count success, \"$2\" occured $3 times
				break
			else
				if [ "x$timecounter" == "x$timeoutend" ]; then
					. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd
					. $srcdir/diag.sh wait-shutdown	# Shutdown rsyslog instance on error 

					echo content-check-with-count failed, expected \"$2\" to occure $3 times, but found it $count times
					echo file rsyslog.out.log content is:
					cat rsyslog.out.log
					. $srcdir/diag.sh error-exit 1
				else
					echo content-check-with-count failed, trying again ...
					./msleep 1000
				fi
			fi
		done
		;;
	 'block-stats-flush')
		echo blocking stats flush
		echo "blockStatsReporting" | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		;;
	 'await-stats-flush-after-block')
		echo unblocking stats flush and waiting for it
		echo "awaitStatsReport" | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		;;
	 'allow-single-stats-flush-after-block-and-wait-for-it')
		echo blocking stats flush
		echo "awaitStatsReport block_again" | ./diagtalker || . $srcdir/diag.sh error-exit  $?
		;;
	 'wait-for-stats-flush')
		echo "will wait for stats push"
		while [[ ! -f $2 ]]; do
				echo waiting for stats file "'$2'" to be created
				./msleep 100
		done
		prev_count=$(cat $2 | grep 'BEGIN$' | wc -l)
		new_count=$prev_count
		while [[ "x$prev_count" == "x$new_count" ]]; do
				new_count=$(cat $2 | grep 'BEGIN$' | wc -l) # busy spin, because it allows as close timing-coordination in actual test run as possible
		done
		echo "stats push registered"
		;;
	 'wait-for-dyn-stats-reset')
		echo "will wait for dyn-stats-reset"
		while [[ ! -f $2 ]]; do
				echo waiting for stats file "'$2'" to be created
				./msleep 100
		done
		prev_purged=$(cat $2 | grep -F 'origin=dynstats' | grep -F "${3}.purge_triggered=" | sed -e 's/.\+.purge_triggered=//g' | awk '{s+=$1} END {print s}')
		new_purged=$prev_purged
		while [[ "x$prev_purged" == "x$new_purged" ]]; do
				new_purged=$(cat $2 | grep -F 'origin=dynstats' | grep -F "${3}.purge_triggered=" | sed -e 's/.\+\.purge_triggered=//g' | awk '{s+=$1} END {print s}') # busy spin, because it allows as close timing-coordination in actual test run as possible
				./msleep 10
		done
		echo "dyn-stats reset for bucket ${3} registered"
		;;
   'content-check')
		# this does a content check which permits regex
		grep "$2" $3
		if [ "$?" -ne "0" ]; then
		    echo "----------------------------------------------------------------------"
		    echo content-check failed to find "'$2'" inside "'$3'"
		    echo "file contents:"
		    cat $3
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'custom-content-check') 
		cat $3 | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed to find "'$2'" inside "'$3'"
		    echo "file contents:"
		    cat $3
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'first-column-sum-check') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ "x${sum}" != "x$5" ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is not equal to expected value of "'$5'"
		    echo "file contents:"
		    cat $4
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'assert-first-column-sum-greater-than') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ ! $sum -gt $5 ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is smaller than expected lower-limit of "'$5'"
		    echo "file contents:"
		    cat $4
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'content-pattern-check') 
		cat rsyslog.out.log | grep -q "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed, not every line matched pattern "'$2'"
		    echo "file contents:"
		    cat $4
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'assert-content-missing') 
		cat rsyslog.out.log | grep -qF "$2"
		if [ "$?" -eq "0" ]; then
				echo content-missing assertion failed, some line matched pattern "'$2'"
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'custom-assert-content-missing') 
		cat $3 | grep -qF "$2"
		if [ "$?" -eq "0" ]; then
				echo content-missing assertion failed, some line in "'$3'" matched pattern "'$2'"
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'gzip-seq-check') # do the usual sequence check, but for gzip files
		rm -f work
		ls -l rsyslog.out.log
		gunzip < rsyslog.out.log | $RS_SORTCMD -g > work
		ls -l work
		# $4... are just to have the abilit to pass in more options...
		./chkseq -fwork -v -s$2 -e$3 $4 $5 $6 $7
		if [ "$?" -ne "0" ]; then
		  echo "sequence error detected"
		  . $srcdir/diag.sh error-exit 1
		fi
		;;
   'nettester') # perform nettester-based tests
   		# use -v for verbose output!
		./nettester -t$2 -i$3 $4
		if [ "$?" -ne "0" ]; then
		  . $srcdir/diag.sh error-exit 1
		fi
		;;
   'setzcat')   # find out name of zcat tool
		if [ `uname` == SunOS ]; then
		   ZCAT=gzcat
		else
		   ZCAT=zcat
		fi
		;;
   'generate-HOSTNAME')   # generate the HOSTNAME file
		. $srcdir/diag.sh startup gethostname.conf
		. $srcdir/diag.sh tcpflood -m1 -M "\"<128>\""
		./msleep 100
		. $srcdir/diag.sh shutdown-when-empty # shut down rsyslogd when done processing messages
		. $srcdir/diag.sh wait-shutdown	# we need to wait until rsyslogd is finished!
		;;
   'generate-conf')   # start a standard test rsyslog.conf
		echo "\$IncludeConfig diag-common.conf" > testconf.conf
		;;
   'add-conf')   # start a standard test rsyslog.conf
		printf "%s" "$2" >> testconf.conf
		;;
   'require-journalctl')   # check if journalctl exists on the system
		if ! hash journalctl 2>/dev/null ; then
		    echo "journalctl command missing, skipping test"
		    exit 77
		fi
		;;
   'download-kafka')
		if [ ! -d $dep_cache_dir ]; then
			echo "Creating dependency cache dir"
			mkdir $dep_cache_dir
		fi
		if [ ! -f $dep_zk_cached_file ]; then
			echo "Downloading zookeeper"
			wget -q $dep_zk_url -O $dep_zk_cached_file
			if [ $? -ne 0 ]
			then
				echo error during wget, retry:
				wget $dep_zk_url -O $dep_zk_cached_file
				if [ $? -ne 0 ]
				then
					. $srcdir/diag.sh error-exit 1
				fi
			fi
		fi
		if [ ! -f $dep_kafka_cached_file ]; then
			echo "Downloading kafka"
			wget -q $dep_kafka_url -O $dep_kafka_cached_file
			if [ $? -ne 0 ]
			then
				echo error during wget, retry:
				wget $dep_kafka_url -O $dep_kafka_cached_file
				if [ $? -ne 0 ]
				then
					. $srcdir/diag.sh error-exit 1
				fi
			fi
		fi
		;;
	 'download-elasticsearch')
		if [ ! -d $dep_cache_dir ]; then
				echo "Creating dependency cache dir"
				mkdir $dep_cache_dir
		fi
		if [ ! -f $dep_es_cached_file ]; then
				echo "Downloading ElasticSearch"
				wget -q $dep_es_url -O $dep_es_cached_file
		fi
		;;
	 'start-zookeeper')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_tk_config="zoo.cfg"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_tk_config="zoo$2.cfg"
		fi

		if [ ! -f $dep_zk_cached_file ]; then
				echo "Dependency-cache does not have zookeeper package, did you download dependencies?"
				exit 77
		fi
		if [ ! -d $dep_work_dir ]; then
				echo "Creating dependency working directory"
				mkdir -p $dep_work_dir
		fi
		if [ -d $dep_work_dir/zk ]; then
				(cd $dep_work_dir/zk && ./bin/zkServer.sh stop)
				./msleep 2000
		fi
		rm -rf $dep_work_dir/zk
		(cd $dep_work_dir && tar -zxvf $dep_zk_cached_file --xform $dep_zk_dir_xform_pattern --show-transformed-names) > /dev/null
		cp $srcdir/testsuites/$dep_work_tk_config $dep_work_dir/zk/conf/zoo.cfg
		echo "Starting Zookeeper instance $2"
		(cd $dep_work_dir/zk && ./bin/zkServer.sh start)
		./msleep 2000
		;;
	 'start-kafka')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_kafka_config="kafka-server.properties"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_kafka_config="kafka-server$2.properties"
		fi

		if [ ! -f $dep_kafka_cached_file ]; then
				echo "Dependency-cache does not have kafka package, did you download dependencies?"
				exit 77
		fi
		if [ ! -d $dep_work_dir ]; then
				echo "Creating dependency working directory"
				mkdir -p $dep_work_dir
		fi
		rm -rf $dep_work_dir/kafka
		(cd $dep_work_dir && tar -zxvf $dep_kafka_cached_file --xform $dep_kafka_dir_xform_pattern --show-transformed-names) > /dev/null
		cp $srcdir/testsuites/$dep_work_kafka_config $dep_work_dir/kafka/config/
		echo "Starting Kafka instance $dep_work_kafka_config"
		(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
		./msleep 4000

		# Check if kafka instance came up!
		kafkapid=`ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}'`
		if [[ "" !=  "$kafkapid" ]];
		then
			echo "Kafka instance $dep_work_kafka_config started with PID $kafkapid"
		else
			echo "Starting Kafka instance $dep_work_kafka_config, SECOND ATTEMPT!"
			(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
			./msleep 4000

			kafkapid=`ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}'`
			if [[ "" !=  "$kafkapid" ]];
			then
				echo "Kafka instance $dep_work_kafka_config started with PID $kafkapid"
			else
				echo "Failed to start Kafka instance for $dep_work_kafka_config"
				. $srcdir/diag.sh error-exit 77
			fi
		fi
		;;
	 'prepare-elasticsearch') # $2, if set, is the number of additional ES instances
		# Heap Size (limit to 128MB for testbench! defaults is way to HIGH)
		export ES_JAVA_OPTS="-Xms128m -Xmx128m"

		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_es_config="es.yml"
			dep_work_es_pidfile="es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_config="es$2.yml"
			dep_work_es_pidfile="es$2.pid"
		fi

		if [ ! -f $dep_es_cached_file ]; then
				echo "Dependency-cache does not have elasticsearch package, did "
				echo "you download dependencies?"
				exit 77
		fi
		if [ ! -d $dep_work_dir ]; then
				echo "Creating dependency working directory"
				mkdir -p $dep_work_dir
		fi
		if [ -d $dep_work_dir/es ]; then
			if [ -e $dep_work_es_pidfile ]; then
				kill -SIGTERM $(cat $dep_work_es_pidfile)
				. $srcdir/diag.sh wait-pid-termination $dep_work_es_pidfile
			fi
		fi
		rm -rf $dep_work_dir/es
		echo TEST USES ELASTICSEARCH BINARY $dep_es_cached_file
		(cd $dep_work_dir && tar -zxf $dep_es_cached_file --xform $dep_es_dir_xform_pattern --show-transformed-names) > /dev/null
		cp $srcdir/testsuites/$dep_work_es_config $dep_work_dir/es/config/elasticsearch.yml

		if [ ! -d $dep_work_dir/es/data ]; then
				echo "Creating elastic search directories"
				mkdir -p $dep_work_dir/es/data
				mkdir -p $dep_work_dir/es/logs
				mkdir -p $dep_work_dir/es/tmp
		fi
		echo ElasticSearch prepared for use in test.
		;;
	 'start-elasticsearch') # $2, if set, is the number of additional ES instances
		# Heap Size (limit to 128MB for testbench! defaults is way to HIGH)
		export ES_JAVA_OPTS="-Xms128m -Xmx128m"

		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_es_config="es.yml"
			dep_work_es_pidfile="es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_config="es$2.yml"
			dep_work_es_pidfile="es$2.pid"
		fi
		echo "Starting ElasticSearch instance $2"
		cd $srcdir 
		# THIS IS THE ACTUAL START of ES
		$dep_work_dir/es/bin/elasticsearch -p $dep_work_es_pidfile -d
		./msleep 2000
		echo "Starting instance $2 started with PID" `cat $dep_work_es_pidfile`

		# Wait for startup with hardcoded timeout
		timeoutend=30
		timeseconds=0
		# Loop until elasticsearch port is reachable or until
		# timeout is reached!
		until [ "`curl --silent --show-error --connect-timeout 1 http://localhost:19200 | grep 'rsyslog-testbench'`" != "" ]; do
			echo "--- waiting for startup: 1 ( $timeseconds ) seconds"
			./msleep 1000
			let "timeseconds = $timeseconds + 1"

			if [ "$timeseconds" -gt "$timeoutend" ]; then 
				echo "--- TIMEOUT ( $timeseconds ) reached!!!"
		                . $srcdir/diag.sh error-exit 1
			fi
		done
		./msleep 2000
		;;
	 'dump-kafka-serverlog')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		if [ ! -d $dep_work_dir/kafka ]; then
			echo "Kafka work-dir $dep_work_dir/kafka does not exist, no kafka debuglog"
		else
			echo "Dumping server.log from Kafka instance $2"
			echo "========================================="
			cat $dep_work_dir/kafka/logs/server.log
			echo "========================================="
		fi
		;;
		
	 'dump-zookeeper-serverlog')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		echo "Dumping zookeeper.out from Zookeeper instance $2"
		echo "========================================="
		cat $dep_work_dir/zk/zookeeper.out
		echo "========================================="
		;;
	 'stop-kafka')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		if [ ! -d $dep_work_dir/kafka ]; then
			echo "Kafka work-dir $dep_work_dir/kafka does not exist, no action needed"
		else
			echo "Stopping Kafka instance $2"
			(cd $dep_work_dir/kafka && ./bin/kafka-server-stop.sh)
			./msleep 2000
			rm -rf $dep_work_dir/kafka
		fi
		;;
	 'stop-zookeeper')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		(cd $dep_work_dir/zk &> /dev/null && ./bin/zkServer.sh stop)
		./msleep 2000
		rm -rf $dep_work_dir/zk
		;;
	 'stop-elasticsearch')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_es_pidfile="es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_pidfile="es$2.pid"
		fi
		if [ -e $dep_work_es_pidfile ]; then
			(cd $srcdir && kill $(cat $dep_work_es_pidfile) )
			. $srcdir/diag.sh wait-pid-termination $dep_work_es_pidfile
		fi
		;;
	 'cleanup-elasticsearch')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_work_es_pidfile="es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_pidfile="es$2.pid"
		fi
		rm -f $dep_work_es_pidfile
		rm -rf $dep_work_dir/es
		;;
	 'create-kafka-topic')
		if [ "x$3" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$3)
		fi
		if [ "x$4" == "x" ]; then
			dep_work_port='2181'
		else
			dep_work_port=$4
		fi
		if [ ! -d $dep_work_dir/kafka ]; then
				echo "Kafka work-dir does not exist, did you start kafka?"
				exit 1
		fi
		if [ "x$2" == "x" ]; then
				echo "Topic-name not provided."
				exit 1
		fi
		(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --create --zookeeper localhost:$dep_work_port/kafka --topic $2 --partitions 2 --replication-factor 1)
		;;
	 'delete-kafka-topic')
		if [ "x$3" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$3)
		fi
		if [ "x$4" == "x" ]; then
			dep_work_port='2181'
		else
			dep_work_port=$4
		fi

		echo "deleting kafka-topic $2"
		(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --delete --zookeeper localhost:$dep_work_port/kafka --topic $2)
		;;
	 'dump-kafka-topic')
		if [ "x$3" == "x" ]; then
			dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
			dep_kafka_log_dump=$(readlink -f $srcdir/rsyslog.out.kafka.log)
		else
			dep_work_dir=$(readlink -f $srcdir/$3)
			dep_kafka_log_dump=$(readlink -f $srcdir/rsyslog.out.kafka$3.log)
		fi
		if [ "x$4" == "x" ]; then
			dep_work_port='2181'
		else
			dep_work_port=$4
		fi

		echo "dumping kafka-topic $2"
		if [ ! -d $dep_work_dir/kafka ]; then
				echo "Kafka work-dir does not exist, did you start kafka?"
				exit 1
		fi
		if [ "x$2" == "x" ]; then
				echo "Topic-name not provided."
				exit 1
		fi

		(cd $dep_work_dir/kafka && ./bin/kafka-console-consumer.sh --timeout-ms 2000 --from-beginning --zookeeper localhost:$dep_work_port/kafka --topic $2 > $dep_kafka_log_dump)
		;;
	'check-inotify') # Check for inotify/fen support 
		if [ -n "$(find /usr/include -name 'inotify.h' -print -quit)" ]; then
			echo [inotify mode]
		elif [ -n "$(find /usr/include/sys/ -name 'port.h' -print -quit)" ]; then
			cat /usr/include/sys/port.h | grep -qF "PORT_SOURCE_FILE" 
			if [ "$?" -ne "0" ]; then
				echo [port.h found but FEN API not implemented , skipping...]
				exit 77 # FEN API not available, skip this test
			fi
			echo [fen mode]
		else
			echo [inotify/fen not supported, skipping...]
			exit 77 # no inotify available, skip this test
		fi
		;;
	'check-inotify-only') # Check for ONLY inotify support 
		if [ -n "$(find /usr/include -name 'inotify.h' -print -quit)" ]; then
			echo [inotify mode]
		else
			echo [inotify not supported, skipping...]
			exit 77 # no inotify available, skip this test
		fi
		;;
	'error-exit') # this is called if we had an error and need to abort. Here, we
                # try to gather as much information as possible. That's most important
		# for systems like Travis-CI where we cannot debug on the machine itself.
		# our $2 is the to-be-used exit code. if $3 is "stacktrace", call gdb.
		if [ -e core* ]
		then
			echo trying to obtain crash location info
			echo note: this may not be the correct file, check it
			CORE=`ls core*`
			echo "bt" >> gdb.in
			echo "q" >> gdb.in
			gdb ../tools/rsyslogd $CORE -batch -x gdb.in
			CORE=
			rm gdb.in
		fi
		if [[ "$3" == 'stacktrace' || ( ! -e IN_AUTO_DEBUG &&  "$USE_AUTO_DEBUG" == 'on' ) ]]; then
			if [ -e core* ]
			then
				echo trying to analyze core for main rsyslogd binary
				echo note: this may not be the correct file, check it
				CORE=`ls core*`
				#echo "set pagination off" >gdb.in
				#echo "core $CORE" >>gdb.in
				echo "bt" >> gdb.in
				echo "echo === THREAD INFO ===" >> gdb.in
				echo "info thread" >> gdb.in
				echo "echo === thread apply all bt full ===" >> gdb.in
				echo "thread apply all bt full" >> gdb.in
				echo "q" >> gdb.in
				gdb ../tools/rsyslogd $CORE -batch -x gdb.in
				CORE=
				rm gdb.in
			fi
		fi
		if [[ ! -e IN_AUTO_DEBUG &&  "$USE_AUTO_DEBUG" == 'on' ]]; then
			touch IN_AUTO_DEBUG
			# OK, we have the testname and can re-run under valgrind
			echo re-running under valgrind control
			current_test="./$(basename $0)" # this path is probably wrong -- theinric
			$current_test
			# wait a little bit so that valgrind can finish
			./msleep 4000
			# next let's try us to get a debug log
			RSYSLOG_DEBUG_SAVE=$RSYSLOG_DEBUG
			export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction"
			$current_test
			./msleep 4000
			RSYSLOG_DEBUG=$RSYSLOG_DEBUG_SAVE
			rm IN_AUTO_DEBUG
		fi
		# Extended debug output for dependencies started by testbench
		if [[ "$EXTRA_EXITCHECK" == 'dumpkafkalogs' ]]; then
			# Dump Zookeeper log
			. $srcdir/diag.sh dump-zookeeper-serverlog
			# Dump Kafka log
			. $srcdir/diag.sh dump-kafka-serverlog
		fi
		exit $2
		;;
   *)		echo "invalid argument" $1
esac
