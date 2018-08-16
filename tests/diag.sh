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
#valgrind="valgrind --tool=helgrind --log-fd=1 --suppressions=$srcdir/linux_localtime_r.supp --gen-suppressions=all"
#valgrind="valgrind --tool=exp-ptrcheck --log-fd=1"
#set -o xtrace
#export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction nostdout"
#export RSYSLOG_DEBUGLOG="log"
TB_TIMEOUT_STARTSTOP=400 # timeout for start/stop rsyslogd in tenths (!) of a second 400 => 40 sec
# note that 40sec for the startup should be sufficient even on very slow machines. we changed this from 2min on 2017-12-12
export RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR="on"  # we want to know when we loose messages due to timeouts
if [ "$srcdir" == "" ]; then
	printf "\$srcdir not set, for a manual build, do 'export srcdir=\$(pwd)'\n"
fi
if [ "$TESTTOOL_DIR" == "" ]; then
	export TESTTOOL_DIR="$srcdir"
fi

# newer functionality is preferrably introduced via bash functions
# rgerhards, 2018-07-03
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
        echo "HTTP endpoint '${http_endpoint}' is not reachable. Skipping test ..."
        exit 77
    else
        echo "HTTP endpoint '${http_endpoint}' is reachable! Starting test ..."
    fi
}

function setvar_RS_HOSTNAME() {
	printf "### Obtaining HOSTNAME (prequisite, not actual test) ###\n"
	generate_conf
	add_conf 'module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="13514")

$template hostname,"%hostname%"
local0.* ./'${RSYSLOG_DYNNAME}'.HOSTNAME;hostname
'
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	startup
	tcpflood -m1 -M "\"<128>\""
	shutdown_when_empty # shut down rsyslogd when done processing messages
	wait_shutdown	# we need to wait until rsyslogd is finished!
	export RS_HOSTNAME="$(cat ${RSYSLOG_DYNNAME}.HOSTNAME)"
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	echo HOSTNAME is: $RS_HOSTNAME
}


# begin a new testconfig
# $1 is the instance id, if given
function generate_conf() {
	new_port="$(get_free_port)"
	if [ "$1" == "" ]; then
		export IMDIAG_PORT=$new_port
		export TESTCONF_NM="${new_port}_" # this basename is also used by instance 2!
		export RSYSLOG_OUT_LOG="rsyslog_${new_port}.out.log"
		export RSYSLOG2_OUT_LOG="rsyslog2_${new_port}.out.log"
		export RSYSLOG_PIDBASE="rsyslog_$(basename $0)_${new_port}"
		export RSYSLOG_DYNNAME="rsyslog_$(basename $0)_${new_port}"
	else
		export IMDIAG_PORT2=$new_port
	fi
	echo imdiag running on port $new_port
	echo "module(load=\"../plugins/imdiag/.libs/imdiag\")
global(inputs.timeout.shutdown=\"10000\")
\$IMDiagServerRun $new_port

:syslogtag, contains, \"rsyslogd\"  ./${RSYSLOG_DYNNAME}$1.started
###### end of testbench instrumentation part, test conf follows:" > ${TESTCONF_NM}$1.conf
}


# add more data to config file. Note: generate_conf must have been called
# $1 is config fragment, $2 the instance id, if given
function add_conf() {
	printf "%s" "$1" >> ${TESTCONF_NM}$2.conf
}


function rst_msleep() {
	$TESTTOOL_DIR/msleep $1
}


# compare file to expected exact content
# $1 is file to compare
function cmp_exact() {
	if [ "$1" == "" ]; then
		printf "Testbench ERROR, cmp_exact() needs filename as \$1\n"
		error_exit  1
	fi
	if [ "$EXPECTED" == "" ]; then
		printf "Testbench ERROR, cmp_exact() needs to have env var EXPECTED set!\n"
		error_exit  1
	fi
	printf "%s\n" "$EXPECTED" | cmp - "$1"
	if [ $? -ne 0 ]; then
		echo "invalid response generated"
		echo "################# $1 is:"
		cat -n ${RSYSLOG_OUT_LOG}
		echo "################# EXPECTED was:"
		printf "%s\n" "$EXPECTED" | cat -n -
		printf "\n#################### diff is:\n"
		printf "%s\n" "$EXPECTED" | diff - "$1"
		error_exit  1
	fi;
}

# code common to all startup...() functions
function startup_common() {
	instance=
	if [ "$1" == "2" ]; then
	    CONF_FILE="${TESTCONF_NM}2.conf"
	    instance=2
	elif [ "$1" == "" -o "$1" == "1" ]; then
	    CONF_FILE="${TESTCONF_NM}.conf"
	else
	    CONF_FILE="$srcdir/testsuites/$1"
	    instance=$2
	fi
	if [ ! -f $CONF_FILE ]; then
	    echo "ERROR: config file '$CONF_FILE' not found!"
	    error_exit 1
	fi
	echo config $CONF_FILE is:
	cat -n $CONF_FILE
}


# start rsyslogd with default params. $1 is the config file name to use
# returns only after successful startup, $2 is the instance (blank or 2!)
# RS_REDIR maybe set to redirect rsyslog output
function startup() {
	startup_common "$1" "$2"
	eval LD_PRELOAD=$RSYSLOG_PRELOAD $valgrind ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$instance.pid -M../runtime/.libs:../.libs -f$CONF_FILE $RS_REDIR &
	. $srcdir/diag.sh wait-startup $instance
}


# same as startup_vg, BUT we do NOT wait on the startup message!
function startup_vg_waitpid_only() {
	if [ "x$RS_TESTBENCH_LEAK_CHECK" == "x" ]; then
	    RS_TESTBENCH_LEAK_CHECK=full
	fi
	if [ "x$1" == "x" ]; then
	    CONF_FILE="${TESTCONF_NM}.conf"
	    echo config $CONF_FILE is:
	    cat -n $CONF_FILE
	else
	    CONF_FILE="$srcdir/testsuites/$1"
	fi
	if [ ! -f $CONF_FILE ]; then
	    echo "ERROR: config file '$CONF_FILE' not found!"
	    exit 1
	fi
	LD_PRELOAD=$RSYSLOG_PRELOAD valgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/known_issues.supp --gen-suppressions=all --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=$RS_TESTBENCH_LEAK_CHECK ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$2.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
	. $srcdir/diag.sh wait-startup-pid $2
}

function startup_vg() {
. $srcdir/diag.sh startup_vg $*
}

function startup_vg_noleak() {
. $srcdir/diag.sh startup_vg_noleak $*
}

function startup_vgthread_waitpid_only() {
. $srcdir/diag.sh startup_vgthread_waitpid_only $*
}

function startup_vgthread() {
. $srcdir/diag.sh startup_vgthread $*
}


# shut rsyslogd down when main queue is empty. $1 is the instance.
function shutdown_when_empty() {
	if [ "$1" == "2" ]
	then
	   echo Shutting down instance 2
	else
	   echo Shutting down instance 1
	fi
	. $srcdir/diag.sh wait-queueempty $1
	if [ "$RSYSLOG_PIDBASE" == "" ]; then
		echo "RSYSLOG_PIDBASE is EMPTY! - bug in test? (instance $1)"
		exit 1
	fi
	cp $RSYSLOG_PIDBASE$1.pid $RSYSLOG_PIDBASE$1.pid.save
	$TESTTOOL_DIR/msleep 500 # wait a bit (think about slow testbench machines!)
	kill `cat $RSYSLOG_PIDBASE$1.pid` # note: we do not wait for the actual termination!
}


# actually, we wait for rsyslog.pid to be deleted.
# $1 is the instance
function wait_shutdown() {
	i=0
	out_pid=`cat $RSYSLOG_PIDBASE$1.pid.save`
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
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
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
	   error_exit  1
	fi
}


# actually, we wait for rsyslog.pid to be deleted. $1 is the instance
function wait_shutdown_vg() {
	wait `cat $RSYSLOG_PIDBASE$1.pid`
	export RSYSLOGD_EXIT=$?
	echo rsyslogd run exited with $RSYSLOGD_EXIT
	if [ -e vgcore.* ]
	then
	   echo "ABORT! core file exists"
	   error_exit 1
	fi
}

# this is called if we had an error and need to abort. Here, we
# try to gather as much information as possible. That's most important
# for systems like Travis-CI where we cannot debug on the machine itself.
# our $1 is the to-be-used exit code. if $2 is "stacktrace", call gdb.
function error_exit() {
	#env
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
	if [[ "$2" == 'stacktrace' || ( ! -e IN_AUTO_DEBUG &&  "$USE_AUTO_DEBUG" == 'on' ) ]]; then
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
		$TESTTOOL_DIR/msleep 4000
		# next let's try us to get a debug log
		RSYSLOG_DEBUG_SAVE=$RSYSLOG_DEBUG
		export RSYSLOG_DEBUG="debug nologfuncflow noprintmutexaction"
		$current_test
		$TESTTOOL_DIR/msleep 4000
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
	# we need to do some minimal cleanup so that "make distcheck" does not
	# complain too much
	rm -f diag-common.conf diag-common2.conf
	exit $1
}


# do the usual sequence check to see if everything was properly received.
# $4... are just to have the abilit to pass in more options...
# add -v to chkseq if you need more verbose output
function seq_check() {
	$RS_SORTCMD -g < ${RSYSLOG_OUT_LOG} | ./chkseq -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1 
	fi
}


# do the usual sequence check to see if everything was properly received. This is
# a duplicateof seq-check, but we could not change its calling conventions without
# breaking a lot of exitings test cases, so we preferred to duplicate the code here.
# $4... are just to have the abilit to pass in more options...
# add -v to chkseq if you need more verbose output
function seq_check2() {
	$RS_SORTCMD -g < ${RSYSLOG2_OUT_LOG}  | ./chkseq -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do the usual sequence check, but for gzip files
# $4... are just to have the abilit to pass in more options...
function gzip_seq_check() {
	ls -l ${RSYSLOG_OUT_LOG}
	gunzip < ${RSYSLOG_OUT_LOG} | $RS_SORTCMD -g | ./chkseq -v -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do a tcpflood run and check if it worked params are passed to tcpflood
function tcpflood() {
	eval ./tcpflood -p$TCPFLOOD_PORT "$@" $TCPFLOOD_EXTRA_OPTS
	if [ "$?" -ne "0" ]; then
		echo "error during tcpflood! see ${RSYSLOG_OUT_LOG}.save for what was written"
		cp ${RSYSLOG_OUT_LOG} ${RSYSLOG_OUT_LOG}.save
		error_exit 1 stacktrace
	fi
}


# cleanup
# detect any left-over hanging instance
function exit_test() {
	nhanging=0
	for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
	do
		echo "ERROR: left-over instance $pid, killing it"
		ps -fp $pid
		pwd
		printf "we do NOT kill the instance as this does not work with multiple\n"
		printf "builds per machine - this message is now informational to show prob exists!\n"
		#kill -9 $pid
		let "nhanging++"
	done
	if test $nhanging -ne 0
	then
	   echo "ABORT! hanging instances left at exit"
	   #error_exit 1
	   exit 77 # for now, just skip - TODO: reconsider when supporting -j
	fi
	# now real cleanup
	rm -f work-*.conf diag-common.conf
	rm -f diag-common2.conf rsyslog.action.*.include
	rm -f work rsyslog.out.* ${RSYSLOG2_OUT_LOG} rsyslog*.pid.save xlate*.lkp_tbl
	rm -rf test-spool test-logdir stat-file1
	rm -f rsyslog.random.data rsyslog.pipe
	rm -f -r rsyslog.input.*
	rm -f rsyslog.input rsyslog.conf.tlscert stat-file1 rsyslog.empty rsyslog.input.* imfile-state*
	rm -rf rsyslog.input-symlink.log rsyslog-link.*.log targets
	rm -f ${TESTCONF_NM}.conf
	rm -f rsyslog.errorfile tmp.qi nocert
	rm -f HOSTNAME imfile-state:.-rsyslog.input
	unset TCPFLOOD_EXTRA_OPTS
	echo  -------------------------------------------------------------------------------
}

# finds a free port that we can bind a listener to
# Obviously, any solution is race as another process could start
# just after us and grab the same port. However, in practice it seems
# to work pretty well. In any case, we should probably call this as
# late as possible before the usage of the port.
function get_free_port() {
python -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'
}

#START: ext kafka config
dep_cache_dir=$(readlink -f .dep_cache)
dep_zk_url=http://www-us.apache.org/dist/zookeeper/zookeeper-3.4.10/zookeeper-3.4.10.tar.gz
dep_zk_cached_file=$dep_cache_dir/zookeeper-3.4.10.tar.gz
dep_kafka_url=http://www-us.apache.org/dist/kafka/0.10.2.1/kafka_2.12-0.10.2.1.tgz
if [ -z "$ES_DOWNLOAD" ]; then
	export ES_DOWNLOAD=elasticsearch-5.6.9.tar.gz
fi
dep_es_cached_file="$dep_cache_dir/$ES_DOWNLOAD"

# kafaka (including Zookeeper)
dep_kafka_cached_file=$dep_cache_dir/kafka_2.12-0.10.2.1.tgz
dep_kafka_dir_xform_pattern='s#^[^/]\+#kafka#g'
dep_zk_dir_xform_pattern='s#^[^/]\+#zk#g'
dep_es_dir_xform_pattern='s#^[^/]\+#es#g'
dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka.log)

#	TODO Make dynamic work dir for multiple instances
dep_work_dir=$(readlink -f .dep_wrk)
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
		for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		do
			echo "ERROR: left-over previous instance $pid, killing it"
			ps -fp $pid
			pwd
			kill -9 $pid
		done
		# cleanup hanging uxsockrcvr processes
		for pid in $(ps -eo pid,args|grep 'uxsockrcvr' |grep -v grep |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		do
			echo "ERROR: left-over previous uxsockrcvr instance $pid, killing it"
			ps -fp $pid
			pwd
			kill -9 $pid
		done
		# end cleanup

		# some default names (later to be set in other parts, once we support fully
		# parallel tests)
		export RSYSLOG_DFLT_LOG_INTERNAL=1 # testbench needs internal messages logged internally!
		export RSYSLOG2_OUT_LOG=rsyslog2.out.log
		export RSYSLOG_OUT_LOG=rsyslog.out.log
		export RSYSLOG_PIDBASE="rsyslog" # TODO: remove
		export RSYSLOG_DYNNAME="rsyslog" # TODO: remove
		export IMDIAG_PORT=13500
		export IMDIAG_PORT2=13501
		export TCPFLOOD_PORT=13514

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
		cp -f $srcdir/testsuites/diag-common.conf diag-common.conf
		cp -f $srcdir/testsuites/diag-common2.conf diag-common2.conf
		rm -f work-*.conf rsyslog.random.data
		rm -f rsyslog*.pid.save xlate*.lkp_tbl
		rm -f log log* # RSyslog debug output 
		rm -f work 
		rm -f rsyslog*.out.log # we need this while the sndrcv tests are not converted
		rm -rf test-spool test-logdir stat-file1
		rm -f rsyslog.pipe rsyslog.input.*
		rm -f rsyslog.input rsyslog.empty rsyslog.input.* imfile-state* omkafka-failed.data
		rm -rf rsyslog.input-symlink.log rsyslog-link.*.log targets
		rm -f HOSTNAME
		rm -f rsyslog.errorfile tmp.qi nocert
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
			export valgrind="valgrind --malloc-fill=ff --free-fill=fe --suppressions=$srcdir/known_issues.supp --log-fd=1"
		fi
		;;

   'check-command-available')   # check if command $2 is available - will exit 77 when not OK
		command -v $2
		if [ $? -ne 0 ] ; then
			echo Testbench requires unavailable command: $2
			exit 77
		fi
		;;
   'check-ipv6-available')   # check if IPv6  - will exit 77 when not OK
		ifconfig -a |grep ::1
		if [ $? -ne 0 ] ; then
			echo this test requires an active IPv6 stack, which we do not have here
			exit 77
		fi
		;;
   'es-init')   # initialize local Elasticsearch *testbench* instance for the next
                # test. NOTE: do NOT put anything useful on that instance!
		curl --silent -XDELETE localhost:${ES_PORT:-9200}/rsyslog_testbench
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
		curl --silent localhost:$es_get_port/rsyslog_testbench/_search?size=$2 > work
		python $srcdir/es_response_get_msgnum.py > ${RSYSLOG_OUT_LOG}
		;;
   'getpid')
		pid=$(cat $RSYSLOG_PIDBASE$2.pid)
		;;
   'startup_vg') # start rsyslogd with default params under valgrind control. $2 is the config file name to use
		# returns only after successful startup, $3 is the instance (blank or 2!)
		startup_vg_waitpid_only $2 $3
		. $srcdir/diag.sh wait-startup $3
		echo startup_vg still running
		;;
   'startup_vg_noleak') # same as startup-vg, except that --leak-check is set to "none". This
   		# is meant to be used in cases where we have to deal with libraries (and such
		# that) we don't can influence and where we cannot provide suppressions as
		# they are platform-dependent. In that case, we can't test for leak checks
		# (obviously), but we can check for access violations, what still is useful.
		RS_TESTBENCH_LEAK_CHECK=no
		startup_vg_waitpid_only $2 $3
		. $srcdir/diag.sh wait-startup $3
		echo startup_vg still running
		;;

   'startup_vgthread_waitpid_only') # same as startup-vgthread, BUT we do NOT wait on the startup message!
		startup_common "$2" "$3"
		valgrind --tool=helgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --suppressions=$srcdir/linux_localtime_r.supp --gen-suppressions=all ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$3.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
		. $srcdir/diag.sh wait-startup-pid $3
		;;
   'startup_vgthread') # start rsyslogd with default params under valgrind thread debugger control.
   		# $2 is the config file name to use
		# returns only after successful startup, $3 is the instance (blank or 2!)
		startup_vgthread_waitpid_only $2 $3
		. $srcdir/diag.sh wait-startup $3
		;;
   'wait-startup-pid') # wait for rsyslogd startup, PID only ($2 is the instance)
		i=0
		while test ! -f $RSYSLOG_PIDBASE$2.pid; do
			$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   ps -f
			   echo "ABORT! Timeout waiting on startup (pid file $RSYSLOG_PIDBASE$2.pid)"
			   error_exit 1
			fi
		done
		echo "rsyslogd$2 started, start msg not yet seen, pid " `cat $RSYSLOG_PIDBASE$2.pid`
		;;
   'wait-startup') # wait for rsyslogd startup ($2 is the instance)
		echo RSYSLOG_DYNNAME: ${RSYSLOG_DYNNAME}
		. $srcdir/diag.sh wait-startup-pid $2
		i=0
		while test ! -f ${RSYSLOG_DYNNAME}$2.started; do
			$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
			ps -p `cat $RSYSLOG_PIDBASE$2.pid` &> /dev/null
			if [ $? -ne 0 ]
			then
			   echo "ABORT! rsyslog pid no longer active during startup!"
			   error_exit 1 stacktrace
			fi
			let "i++"
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on startup ('${RSYSLOG_DYNNAME}.started' file)"
			   error_exit 1
			fi
		done
		echo "rsyslogd$2 startup msg seen, pid " `cat $RSYSLOG_PIDBASE$2.pid`
		;;
   'wait-pid-termination')  # wait for the pid in pid $2 to terminate, abort on timeout
		i=0
		out_pid=$2
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
			$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
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
			echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
		else
			echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		fi
		;;
   'wait-queueempty') # wait for main message queue to be empty. $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
		else
			echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		fi
		;;
   'await-lookup-table-reload') # wait for all pending lookup table reloads to complete $2 is the instance.
		if [ "$2" == "2" ]
		then
			echo AwaitLookupTableReload | $TESTTOOL_DIR/diagtalker -pIMDIAG_PORT2 || error_exit  $?
		else
			echo AwaitLookupTableReload | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		fi
		;;
   'issue-HUP') # shut rsyslogd down when main queue is empty. $2 is the instance.
		kill -HUP `cat $RSYSLOG_PIDBASE$2.pid`
		$TESTTOOL_DIR/msleep 1000
		;;
   'shutdown-immediate') # shut rsyslogd down without emptying the queue. $2 is the instance.
		cp $RSYSLOG_PIDBASE$2.pid $RSYSLOG_PIDBASE$2.pid.save
		kill `cat $RSYSLOG_PIDBASE$2.pid`
		# note: we do not wait for the actual termination!
		;;
   'kill-immediate') # kill rsyslog unconditionally
		kill -9 `cat $RSYSLOG_PIDBASE.pid`
		# note: we do not wait for the actual termination!
		;;
   'injectmsg') # inject messages via our inject interface (imdiag)
		echo injecting $3 messages
		echo injectmsg $2 $3 $4 $5 | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		# TODO: some return state checking? (does it really make sense here?)
		;;
   'injectmsg2') # inject messages in INSTANCE 2 via our inject interface (imdiag)
		echo injecting $3 messages
		echo injectmsg $2 $3 $4 $5 | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
		# TODO: some return state checking? (does it really make sense here?)
		;;
    'injectmsg-litteral') # inject litteral-payload  via our inject interface (imdiag)
		echo injecting msg payload from: $2
    cat $2 | sed -e 's/^/injectmsg litteral /g' | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		# TODO: some return state checking? (does it really make sense here?)
		;;
   'check-mainq-spool') # check if mainqueue spool files exist, if not abort (we just check .qi).
		echo There must exist some files now:
		ls -l test-spool
		echo .qi file:
		cat test-spool/mainq.qi
		if test ! -f test-spool/mainq.qi; then
		  echo "error: mainq.qi does not exist where expected to do so!"
		  error_exit 1
		fi
		;;
   'presort')	# sort the output file just like we normally do it, but do not call
		# seqchk. This is needed for some operations where we need the sort
		# result for some preprocessing. Note that a later seqchk will sort
		# again, but that's not an issue.
		rm -f work
		$RS_SORTCMD -g < ${RSYSLOG_OUT_LOG} > work
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
		  error_exit 1
		fi
		;;
   'ensure-no-process-exists')
    ps -ef | grep -v grep | grep -qF "$2"
    if [ "x$?" == "x0" ]; then
      echo "assertion failed: process with name-fragment matching '$2' found"
		  error_exit 1
    fi
    ;;
   'grep-check') # grep for "$EXPECTED" present in rsyslog.log - env var must be set
		 # before this method is called
		grep "$EXPECTED" ${RSYSLOG_OUT_LOG} > /dev/null
		if [ $? -eq 1 ]; then
		  echo "GREP FAIL: ${RSYSLOG_OUT_LOG} content:"
		  cat ${RSYSLOG_OUT_LOG}
		  echo "GREP FAIL: expected text not found:"
		  echo "$EXPECTED"
		error_exit 1
		fi;
		;;
   'content-cmp')
		echo "$2" | cmp - ${RSYSLOG_OUT_LOG}
		if [ "$?" -ne "0" ]; then
		    echo FAIL: content-cmp failed
		    echo EXPECTED:
		    echo $2
		    echo ACTUAL:
		    cat ${RSYSLOG_OUT_LOG}
		    error_exit 1
		fi
		;;
   'content-check')
		cat ${RSYSLOG_OUT_LOG} | grep -qF "$2"
		if [ "$?" -ne "0" ]; then
		    printf "\n============================================================\n"
		    echo FAIL: content-check failed to find "'$2'", content is
		    cat ${RSYSLOG_OUT_LOG}
		    error_exit 1
		fi
		;;
   'wait-file-lines') 
		# $2 filename, $3 expected nbr of lines, $4 nbr of tries
		if [ "$4" == "" ]; then
			timeoutend=1
		else
			timeoutend=$4
		fi
		timecounter=0

		while [  $timecounter -lt $timeoutend ]; do
			let timecounter=timecounter+1

			count=$(wc -l < ${RSYSLOG_OUT_LOG})
			if [ $count -eq $3 ]; then
				echo wait-file-lines success, have $3 lines
				break
			else
				if [ "x$timecounter" == "x$timeoutend" ]; then
					echo wait-file-lines failed, expected $3 got $count
					shutdown_when_empty
					wait_shutdown
					error_exit 1
				else
					echo wait-file-lines not yet there, currently $count lines
					$TESTTOOL_DIR/msleep 200
				fi
			fi
		done
		unset count
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
			let timecounter=timecounter+1

			count=$(cat ${RSYSLOG_OUT_LOG} | grep -F "$2" | wc -l)

			if [ $count -eq $3 ]; then
				echo content-check-with-count success, \"$2\" occured $3 times
				break
			else
				if [ "x$timecounter" == "x$timeoutend" ]; then
					shutdown_when_empty # shut down rsyslogd
					wait_shutdown	# Shutdown rsyslog instance on error 

					echo content-check-with-count failed, expected \"$2\" to occur $3 times, but found it $count times
					echo file ${RSYSLOG_OUT_LOG} content is:
					sort < ${RSYSLOG_OUT_LOG}
					error_exit 1
				else
					echo content-check-with-count have $count, wait for $3 times $2...
					$TESTTOOL_DIR/msleep 1000
				fi
			fi
		done
		;;
	 'block-stats-flush')
		echo blocking stats flush
		echo "blockStatsReporting" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		;;
	 'await-stats-flush-after-block')
		echo unblocking stats flush and waiting for it
		echo "awaitStatsReport" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		;;
	 'allow-single-stats-flush-after-block-and-wait-for-it')
		echo blocking stats flush
		echo "awaitStatsReport block_again" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		;;
	 'wait-for-stats-flush')
		echo "will wait for stats push"
		while [[ ! -f $2 ]]; do
				echo waiting for stats file "'$2'" to be created
				$TESTTOOL_DIR/msleep 100
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
				$TESTTOOL_DIR/msleep 100
		done
		prev_purged=$(cat $2 | grep -F 'origin=dynstats' | grep -F "${3}.purge_triggered=" | sed -e 's/.\+.purge_triggered=//g' | awk '{s+=$1} END {print s}')
		new_purged=$prev_purged
		while [[ "x$prev_purged" == "x$new_purged" ]]; do
				new_purged=$(cat $2 | grep -F 'origin=dynstats' | grep -F "${3}.purge_triggered=" | sed -e 's/.\+\.purge_triggered=//g' | awk '{s+=$1} END {print s}') # busy spin, because it allows as close timing-coordination in actual test run as possible
				$TESTTOOL_DIR/msleep 10
		done
		echo "dyn-stats reset for bucket ${3} registered"
		;;
   'content-check-regex')
		# this does a content check which permits regex
		grep "$2" $3 -q
		if [ "$?" -ne "0" ]; then
		    echo "----------------------------------------------------------------------"
		    echo FAIL: content-check-regex failed to find "'$2'" inside "'$3'"
		    echo "file contents:"
		    cat $3
		    error_exit 1
		fi
		;;
   'custom-content-check') 
   		set -x
		#cat $3 | grep -qF "$2"
		cat $3 | grep -F "$2"
		if [ "$?" -ne "0" ]; then
		    echo FAIL: custom-content-check failed to find "'$2'" inside "'$3'"
		    echo "file contents:"
		    cat $3
		    error_exit 1
		fi
		set +x
		;;
   'first-column-sum-check') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ "x${sum}" != "x$5" ]; then
		    printf "\n============================================================\n"
		    echo FAIL: sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is NOT equal to EXPECTED value of "'$5'"
		    echo "file contents:"
		    cat $4
		    error_exit 1
		fi
		;;
   'assert-first-column-sum-greater-than') 
		sum=$(cat $4 | grep $3 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ ! $sum -gt $5 ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is smaller than expected lower-limit of "'$5'"
		    echo "file contents:"
		    cat $4
		    error_exit 1
		fi
		;;
   'content-pattern-check') 
		cat ${RSYSLOG_OUT_LOG} | grep -q "$2"
		if [ "$?" -ne "0" ]; then
		    echo content-check failed, not every line matched pattern "'$2'"
		    echo "file contents:"
		    cat $4
		    error_exit 1
		fi
		;;
   'assert-content-missing') 
		cat ${RSYSLOG_OUT_LOG} | grep -qF "$2"
		if [ "$?" -eq "0" ]; then
				echo content-missing assertion failed, some line matched pattern "'$2'"
		    error_exit 1
		fi
		;;
   'custom-assert-content-missing') 
		cat $3 | grep -qF "$2"
		if [ "$?" -eq "0" ]; then
				echo content-missing assertion failed, some line in "'$3'" matched pattern "'$2'"
		    error_exit 1
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
		startup gethostname.conf
		tcpflood -m1 -M "\"<128>\""
		$TESTTOOL_DIR/msleep 100
		shutdown_when_empty # shut down rsyslogd when done processing messages
		wait_shutdown	# we need to wait until rsyslogd is finished!
		;;
   'require-journalctl')   # check if journalctl exists on the system
		if ! hash journalctl 2>/dev/null ; then
		    echo "journalctl command missing, skipping test"
		    exit 77
		fi
		;;
   'download-kafka')
		if [ ! -d $dep_cache_dir ]; then
			echo "Creating dependency cache dir $dep_cache_dir"
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
					error_exit 1
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
					error_exit 1
				fi
			fi
		fi
		;;
	 'download-elasticsearch')
		if [ ! -d $dep_cache_dir ]; then
				echo "Creating dependency cache dir $dep_cache_dir"
				mkdir $dep_cache_dir
		fi
		if [ ! -f $dep_es_cached_file ]; then
				if [ -f /local_dep_cache/$ES_DOWNLOAD ]; then
					printf "ElasticSearch: satisfying dependency %s from system cache.\n" "$ES_DOWNLOAD"
					cp /local_dep_cache/$ES_DOWNLOAD $dep_es_cached_file
				else
					dep_es_url="https://artifacts.elastic.co/downloads/elasticsearch/$ES_DOWNLOAD"
					printf "ElasticSearch: satisfying dependency %s from %s\n" "$ES_DOWNLOAD" "$dep_es_url"
					wget -q $dep_es_url -O $dep_es_cached_file
				fi
		fi
		;;
	 'start-zookeeper')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
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
				$TESTTOOL_DIR/msleep 2000
		fi
		rm -rf $dep_work_dir/zk
		(cd $dep_work_dir && tar -zxvf $dep_zk_cached_file --xform $dep_zk_dir_xform_pattern --show-transformed-names) > /dev/null
		cp -f $srcdir/testsuites/$dep_work_tk_config $dep_work_dir/zk/conf/zoo.cfg
		echo "Starting Zookeeper instance $2"
		(cd $dep_work_dir/zk && ./bin/zkServer.sh start)
		$TESTTOOL_DIR/msleep 2000
		;;
	 'start-kafka')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
			dep_work_kafka_config="kafka-server.properties"
		else
			dep_work_dir=$(readlink -f $2)
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
		cp -f $srcdir/testsuites/$dep_work_kafka_config $dep_work_dir/kafka/config/
		echo "Starting Kafka instance $dep_work_kafka_config"
		(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
		$TESTTOOL_DIR/msleep 4000

		# Check if kafka instance came up!
		kafkapid=`ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}'`
		if [[ "" !=  "$kafkapid" ]];
		then
			echo "Kafka instance $dep_work_kafka_config started with PID $kafkapid"
		else
			echo "Starting Kafka instance $dep_work_kafka_config, SECOND ATTEMPT!"
			(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
			$TESTTOOL_DIR/msleep 4000

			kafkapid=`ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}'`
			if [[ "" !=  "$kafkapid" ]];
			then
				echo "Kafka instance $dep_work_kafka_config started with PID $kafkapid"
			else
				echo "Failed to start Kafka instance for $dep_work_kafka_config"
				error_exit 77
			fi
		fi
		;;
	 'prepare-elasticsearch') # $2, if set, is the number of additional ES instances
		# Heap Size (limit to 128MB for testbench! defaults is way to HIGH)
		export ES_JAVA_OPTS="-Xms128m -Xmx128m"

		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
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
		                error_exit 1
		fi
		if [ ! -d $dep_work_dir ]; then
				echo "Creating dependency working directory"
				mkdir -p $dep_work_dir
		fi
		if [ -d $dep_work_dir/es ]; then
			if [ -e $dep_work_es_pidfile ]; then
				es_pid = $(cat $dep_work_es_pidfile)
				kill -SIGTERM $es_pid
				. $srcdir/diag.sh wait-pid-termination $es_pid
			fi
		fi
		rm -rf $dep_work_dir/es
		echo TEST USES ELASTICSEARCH BINARY $dep_es_cached_file
		(cd $dep_work_dir && tar -zxf $dep_es_cached_file --xform $dep_es_dir_xform_pattern --show-transformed-names) > /dev/null
		if [ -n "${ES_PORT:-}" ] ; then
			rm -f $dep_work_dir/es/config/elasticsearch.yml
			sed "s/^http.port:.*\$/http.port: ${ES_PORT}/" $srcdir/testsuites/$dep_work_es_config > $dep_work_dir/es/config/elasticsearch.yml
		else
			cp -f $srcdir/testsuites/diag-common.conf diag-common.conf
			cp -f $srcdir/testsuites/$dep_work_es_config $dep_work_dir/es/config/elasticsearch.yml
		fi

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

		pwd
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
			dep_work_es_config="es.yml"
			dep_work_es_pidfile="$(pwd)/es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_config="es$2.yml"
			dep_work_es_pidfile="es$2.pid"
		fi
		echo "Starting ElasticSearch instance $2"
		# THIS IS THE ACTUAL START of ES
		$dep_work_dir/es/bin/elasticsearch -p $dep_work_es_pidfile -d
		$TESTTOOL_DIR/msleep 2000
		echo "Starting instance $2 started with PID" `cat $dep_work_es_pidfile`

		# Wait for startup with hardcoded timeout
		timeoutend=60
		timeseconds=0
		# Loop until elasticsearch port is reachable or until
		# timeout is reached!
		until [ "`curl --silent --show-error --connect-timeout 1 http://localhost:${ES_PORT:-19200} | grep 'rsyslog-testbench'`" != "" ]; do
			echo "--- waiting for ES startup: $timeseconds seconds"
			$TESTTOOL_DIR/msleep 1000
			let "timeseconds = $timeseconds + 1"

			if [ "$timeseconds" -gt "$timeoutend" ]; then 
				echo "--- TIMEOUT ( $timeseconds ) reached!!!"
		                error_exit 1
			fi
		done
		$TESTTOOL_DIR/msleep 2000
		echo ES startup succeeded
		;;
	 'dump-kafka-serverlog')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
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
			dep_work_dir=$(readlink -f .dep_wrk)
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
			dep_work_dir=$(readlink -f .dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		if [ ! -d $dep_work_dir/kafka ]; then
			echo "Kafka work-dir $dep_work_dir/kafka does not exist, no action needed"
		else
			echo "Stopping Kafka instance $2"
			(cd $dep_work_dir/kafka && ./bin/kafka-server-stop.sh)
			$TESTTOOL_DIR/msleep 2000
			rm -rf $dep_work_dir/kafka
		fi
		;;
	 'stop-zookeeper')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
		fi
		(cd $dep_work_dir/zk &> /dev/null && ./bin/zkServer.sh stop)
		$TESTTOOL_DIR/msleep 2000
		rm -rf $dep_work_dir/zk
		;;
	 'stop-elasticsearch')
		if [ "x$2" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
			dep_work_es_pidfile="es.pid"
		else
			dep_work_dir=$(readlink -f $srcdir/$2)
			dep_work_es_pidfile="es$2.pid"
		fi
		if [ -e $dep_work_es_pidfile ]; then
			es_pid=$(cat $dep_work_es_pidfile)
			printf "stopping ES with pid %d\n" $es_pid
			kill -SIGTERM $es_pid
			. $srcdir/diag.sh wait-pid-termination $es_pid
		fi
		;;
	 'cleanup-elasticsearch')
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_es_pidfile="es.pid"
		. $srcdir/diag.sh stop-elasticsearch
		rm -f $dep_work_es_pidfile
		rm -rf $dep_work_dir/es
		;;
	 'create-kafka-topic')
		if [ "x$3" == "x" ]; then
			dep_work_dir=$(readlink -f .dep_wrk)
		else
			dep_work_dir=$(readlink -f $3)
		fi
		if [ "x$4" == "x" ]; then
			dep_work_port='2181'
		else
			dep_work_port=$4
		fi
		if [ ! -d $dep_work_dir/kafka ]; then
				echo "Kafka work-dir $dep_work_dir/kafka does not exist, did you start kafka?"
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
			dep_work_dir=$(readlink -f .dep_wrk)
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
			dep_work_dir=$(readlink -f .dep_wrk)
			dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka.log)
		else
			dep_work_dir=$(readlink -f $srcdir/$3)
			dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka$3.log)
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
   *)		echo "invalid argument" $1
esac
