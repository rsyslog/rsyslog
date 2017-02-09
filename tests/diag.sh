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
#valgrind="valgrind --tool=helgrind --log-fd=1"
#valgrind="valgrind --tool=exp-ptrcheck --log-fd=1"
#set -o xtrace
export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
export RSYSLOG_DEBUGLOG="log"
TB_TIMEOUT_STARTSTOP=3000 # timeout for start/stop rsyslogd in tenths (!) of a second 3000 => 5 min

#START: ext dependency config
dep_zk_url=http://www-us.apache.org/dist/zookeeper/zookeeper-3.4.8/zookeeper-3.4.8.tar.gz
dep_kafka_url=http://www-us.apache.org/dist/kafka/0.9.0.1/kafka_2.11-0.9.0.1.tgz
dep_cache_dir=$(readlink -f $srcdir/.dep_cache)
dep_work_dir=$(readlink -f $srcdir/.dep_wrk)
dep_zk_cached_file=$dep_cache_dir/zookeeper-3.4.8.tar.gz
dep_kafka_cached_file=$dep_cache_dir/kafka_2.11-0.9.0.1.tgz

dep_kafka_work_dir=$dep_work_dir/kafka
dep_kafka_dir_xform_pattern='s#^[^/]\+#kafka#g'

dep_zk_work_dir=$dep_work_dir/zk
dep_zk_dir_xform_pattern='s#^[^/]\+#zk#g'

dep_kafka_log_dump=$(readlink -f $srcdir/rsyslog.out.kafka.log)
#END: ext dependency config

case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		if [ -z $RS_SORTCMD ]; then
			RS_SORTCMD=sort
		fi  
		ulimit -c unlimited  &> /dev/null # at least try to get core dumps
		echo "------------------------------------------------------------"
		echo "Test: $0"
		echo "------------------------------------------------------------"
		cp $srcdir/testsuites/diag-common.conf diag-common.conf
		cp $srcdir/testsuites/diag-common2.conf diag-common2.conf
		rm -f rsyslogd.started work-*.conf rsyslog.random.data
		rm -f rsyslogd2.started work-*.conf
		rm -f work rsyslog.out.log rsyslog2.out.log rsyslog.out.log.save # common work files
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.out.*.log work-presort rsyslog.pipe
		rm -f -r rsyslog.input.*
		rm -f rsyslog.input rsyslog.empty rsyslog.input.* imfile-state*
		rm -f testconf.conf HOSTNAME
		rm -f rsyslog.errorfile tmp.qi
		rm -f core.* vgcore.*
		# Note: rsyslog.action.*.include must NOT be deleted, as it
		# is used to setup some parameters BEFORE calling init. This
		# happens in chained test scripts. Delete on exit is fine,
		# though.
		mkdir test-spool
		ulimit -c 4000000000
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
		for pid in $(ps -eo pid,cmd|grep '/tools/[r]syslogd' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
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
   'es-init')   # initialize local Elasticsearch *testbench* instance for the next
                # test. NOTE: do NOT put anything useful on that instance!
		curl -XDELETE localhost:9200/rsyslog_testbench
		;;
   'es-getdata') # read data from ES to a local file so that we can process
   		# it with out regular tooling.
		# Note: param 2 MUST be number of records to read (ES does
		# not return the full set unless you tell it explicitely).
		curl localhost:9200/rsyslog_testbench/_search?size=$2 > work
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
   'startup-vg') # start rsyslogd with default params under valgrind control. $2 is the config file name to use
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
		valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=full ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
		. $srcdir/diag.sh wait-startup $3
		echo startup-vg still running
		;;
   'startup-vg-noleak') # same as startup-vg, except that --leak-check is set to "none". This
   		# is meant to be used in cases where we have to deal with libraries (and such
		# that) we don't can influence and where we cannot provide suppressions as
		# they are platform-dependent. In that case, we can't test for leak checks
		# (obviously), but we can check for access violations, what still is useful.
		if [ ! -f $srcdir/testsuites/$2 ]; then
		    echo "ERROR: config file '$srcdir/testsuites/$2' not found!"
		    exit 1
		fi
		valgrind $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=no ../tools/rsyslogd -C -n -irsyslog$3.pid -M../runtime/.libs:../.libs -f$srcdir/testsuites/$2 &
		. $srcdir/diag.sh wait-startup $3
		echo startup-vg still running
		;;
	 'msleep')
   	$srcdir/msleep $2
		;;

   'wait-startup') # wait for rsyslogd startup ($2 is the instance)
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
		i=0
		while test ! -f rsyslogd$2.started; do
			./msleep 100 # wait 100 milliseconds
			ps -p `cat rsyslog$2.pid`
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
		echo "rsyslogd$2 started with pid " `cat rsyslog$2.pid`
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
		kill `cat rsyslog.pid`
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
		  . $srcdir/diag.sh error-exit 1
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
		if test ! -f test-spool/mainq.qi; then
		  echo "error: mainq.qi does not exist where expected to do so!"
		  ls -l test-spool
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
   'content-check') 
		cat rsyslog.out.log | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed to find "'$2'", content is
		    cat rsyslog.out.log
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'content-check-with-count') 
		count=$(cat rsyslog.out.log | grep -F "$2" | wc -l)
		if [ "x$count" == "x$3" ]; then
		    echo content-check-with-count success, \"$2\" occured $3 times
		else
		    echo content-check-with-count failed, expected \"$2\" to occure $3 times, but found it $count times
		    . $srcdir/diag.sh error-exit 1
		fi
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
   'custom-content-check') 
		cat $3 | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed to find "'$2'" inside "'$3'"
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'first-column-sum-check') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ "x${sum}" != "x$5" ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is not equal to expected value of "'$5'"
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'assert-first-column-sum-greater-than') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ ! $sum -gt $5 ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is smaller than expected lower-limit of "'$5'"
		    . $srcdir/diag.sh error-exit 1
		fi
		;;
   'content-pattern-check') 
		cat rsyslog.out.log | grep -q "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed, not every line matched pattern "'$2'"
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
		echo "$2" >> testconf.conf
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
				wget $dep_zk_url -O $dep_zk_cached_file
		fi
		if [ ! -f $dep_kafka_cached_file ]; then
				echo "Downloading kafka"
				wget $dep_kafka_url -O $dep_kafka_cached_file
		fi
		;;
	 'start-kafka')
		if [ ! -f $dep_zk_cached_file ]; then
				echo "Dependency-cache does not have zookeeper package, did you download dependencies?"
				exit 1
		fi
		if [ ! -f $dep_kafka_cached_file ]; then
				echo "Dependency-cache does not have kafka package, did you download dependencies?"
				exit 1
		fi
		if [ ! -d $dep_work_dir ]; then
				echo "Creating dependency working directory"
				mkdir -p $dep_work_dir
		fi
		if [ -d $dep_kafka_work_dir ]; then
				(cd $dep_kafka_work_dir && ./bin/kafka-server-stop.sh)
				./msleep 4000
		fi
		if [ -d $dep_zk_work_dir ]; then
				(cd $dep_zk_work_dir && ./bin/zkServer.sh stop)
				./msleep 2000
		fi
		rm -rf $dep_kafka_work_dir
		rm -rf $dep_zk_work_dir
		(cd $dep_work_dir && tar -zxvf $dep_zk_cached_file --xform $dep_zk_dir_xform_pattern --show-transformed-names)
		(cd $dep_work_dir && tar -zxvf $dep_kafka_cached_file --xform $dep_kafka_dir_xform_pattern --show-transformed-names)
		cp $srcdir/testsuites/zoo.cfg $dep_zk_work_dir/conf/
		(cd $dep_zk_work_dir && ./bin/zkServer.sh start)
		./msleep 4000
		cp $srcdir/testsuites/kafka-server.properties $dep_kafka_work_dir/config/
		(cd $dep_kafka_work_dir && ./bin/kafka-server-start.sh -daemon ./config/kafka-server.properties)
		./msleep 8000
		;;
	 'stop-kafka')
		if [ ! -d $dep_kafka_work_dir ]; then
				echo "Kafka work-dir does not exist, did you start kafka?"
				exit 1
		fi
		(cd $dep_kafka_work_dir && ./bin/kafka-server-stop.sh)
		./msleep 4000
		(cd $dep_zk_work_dir && ./bin/zkServer.sh stop)
		./msleep 2000
		rm -rf $dep_kafka_work_dir
		rm -rf $dep_zk_work_dir
		;;
	 'create-kafka-topic')
		if [ ! -d $dep_kafka_work_dir ]; then
				echo "Kafka work-dir does not exist, did you start kafka?"
				exit 1
		fi
		if [ "x$2" == "x" ]; then
				echo "Topic-name not provided."
				exit 1
		fi
		(cd $dep_kafka_work_dir && ./bin/kafka-topics.sh --create --zookeeper localhost:2181/kafka --topic $2 --partitions 2 --replication-factor 1)
		;;
	 'dump-kafka-topic')
		echo "dumping kafka-topic $2"
		if [ ! -d $dep_kafka_work_dir ]; then
				echo "Kafka work-dir does not exist, did you start kafka?"
				exit 1
		fi
		if [ "x$2" == "x" ]; then
				echo "Topic-name not provided."
				exit 1
		fi

		(cd $dep_kafka_work_dir && ./bin/kafka-console-consumer.sh --timeout-ms 2000 --from-beginning --zookeeper localhost:2181/kafka --topic $2 > $dep_kafka_log_dump)
		;;
   'error-exit') # this is called if we had an error and need to abort. Here, we
                # try to gather as much information as possible. That's most important
		# for systems like Travis-CI where we cannot debug on the machine itself.
		# our $2 is the to-be-used exit code. if $3 is "stacktrace", call gdb.
		if [[ "$3" == 'stacktrace' || ( ! -e IN_AUTO_DEBUG &&  "$USE_AUTO_DEBUG" == 'on' ) ]]; then
			if [ -e core* ]
			then
				echo trying to analyze core for main rsyslogd binary
				echo note: this may not be the correct file, check it
				CORE=`ls core*`
				echo "set pagination off" >gdb.in
				echo "core $CORE" >>gdb.in
				echo "info thread" >> gdb.in
				echo "thread apply all bt full" >> gdb.in
				echo "q" >> gdb.in
				gdb ../tools/rsyslogd < gdb.in
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
		exit $2
		;;
   *)		echo "invalid argument" $1
esac
