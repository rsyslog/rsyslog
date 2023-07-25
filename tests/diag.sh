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
# SUDO		either blank, or the command to use for sudo (usually "sudo").
#		This env var is sometimes used to increase the chance that we
#		get extra information, e.g. when trying to analyze running
#		processes. Bottom line: if sudo is possible and you are OK with
#		the testbench utilizing this, use
#		export SUDO=sudo
#		to activate the extra functionality.
# RS_SORTCMD    Sort command to use (must support $RS_SORT_NUMERIC_OPT option). If unset,
#		"sort" is used. E.g. Solaris needs "gsort"
# RS_SORT_NUMERIC_OPT option to use for numerical sort, If unset "-g" is used.
# RS_CMPCMD     cmp command to use. If unset, "cmd" is used.
#               E.g. Solaris needs "gcmp"
# RS_HEADCMD    head command to use. If unset, "head" is used.
#               E.g. Solaris needs "ghead"
# RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT
#		global input timeout shutdown, default 60000 (60) sec. This should
#		only be set specifically if there is good need to do so, e.g. if
#		a test needs to timeout.
# USE_VALGRIND  if set to "YES", the test will be run under valgrind control, with
#               "default" settings (memcheck including leak check, termination on error).
#		This permits to have valgrind and non-valgrind versions of the same test
#		without the need to write it twice: just have a 2-liner -vg.sh which
#		does:
#			export USE_VALGRIND="YES"
#			source original-test.sh
#		sample can be seen in imjournal-basic[.vg].sh
#		You may also use USE_VALGRIND="YES-NOLEAK" to request valgrind without
#		leakcheck (this sometimes is needed).
# ABORT_ALL_ON_TEST_FAIL
#		if set to "YES" and one test fails, all others are not executed but skipped.
#		This is useful in long-running CI jobs where we are happy with seeing the
#		first failure (to save time).
#
#
# EXIT STATES
# 0 - ok
# 1 - FAIL
# 77 - SKIP
# 100 - Testbench failure
export TB_ERR_TIMEOUT=101
# 177 - internal state: test failed, but in a way that makes us strongly believe
#       this is caused by environment. This will lead to exit 77 (SKIP), but report
#       the failure if failure reporting is active

# environment variables:
# USE_AUTO_DEBUG "on" --> enables automatic debugging, anything else
#                turns it off

# diag system internal environment variables
# these variables are for use by test scripts - they CANNOT be
# overridden by the user
# TCPFLOOD_EXTRA_OPTS   enables to set extra options for tcpflood, usually
#                       used in tests that have a common driver where it
#                       is too hard to set these options otherwise
# CONFIG
export ZOOPIDFILE="$(pwd)/zookeeper.pid"

#valgrind="valgrind --malloc-fill=ff --free-fill=fe --log-fd=1"
#valgrind="valgrind --tool=callgrind" # for kcachegrind profiling

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
TB_TEST_TIMEOUT=90  # number of seconds after which test checks timeout (eg. waits)
TB_TEST_MAX_RUNTIME=${TEST_MAX_RUNTIME:-580} # maximum runtime in seconds for a test;
			# default TEST_MAX_RUNTIME e.g. for long-running tests or special
			# testbench use. Testbench will abort test
			# after that time (iff it has a chance to, not strictly enforced)
			# Note: 580 is slightly below the rsyslog-ci required max non-stdout writing timeout
			# This is usually at 600 (10 minutes) and processes will be force-terminated if they
			# go over it. This is especially bad because we do not receive notifications in this
			# case.
export RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR="on"  # we want to know when we loose messages due to timeouts
if [ "$TESTTOOL_DIR" == "" ]; then
	export TESTTOOL_DIR="${srcdir:-.}"
fi

# newer functionality is preferably introduced via bash functions
# rgerhards, 2018-07-03
rsyslog_testbench_test_url_access() {
    local missing_requirements=
    if ! hash curl 2>/dev/null ; then
        missing_requirements="'curl' is missing in PATH; Make sure you have cURL installed! Skipping test ..."
    fi

    if [ -n "${missing_requirements}" ]; then
        printf '%s\n' "${missing_requirements}"
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

# function to skip a test on a specific platform
# $1 is what we check in uname, $2 (optional) is a reason message
skip_platform() {
	if [ "$(uname)" == "$1" ]; then
		printf 'platform is "%s" - test does not work under "%s"\n' "$(uname)" "$1"
		if [ "$2" != "" ]; then
			printf 'reason: %s\n' "$2"
		fi
		exit 77
	fi

}


# a consistent format to output testbench timestamps
tb_timestamp() {
	printf '%s[%s] ' "$(date +%H:%M:%S)" "$(( $(date +%s) - TB_STARTTEST ))"
}

# override the test timeout, but only if the new value is higher
# than the previous one. This is necessary for slow test systems
# $1 is timeout in seconds
override_test_timeout() {
	if [ "${1:=0}" == "" ]; then
		printf 'FAIL: invalid testbench call, override_test_timeout needs value\n'
		error_exit 100
	fi
	if [ "$1" -gt "$TB_TEST_TIMEOUT" ]; then
		TB_TEST_TIMEOUT=$1
		printf 'info: TB_TEST_TIMEOUT increased to %s\n' "$TB_TEST_TIMEOUT"
	fi
}

# set special tests status. States ($1) are:
# unreliable -- as the name says, test does not work reliably; $2 must be github issue URL
#               depending on CI configuration, "unreliable" tests are skipped and not failed
#               or not executed at all. Test reports may also be amended to github issue.
test_status() {
	if [ "$1" == "unreliable" ]; then
		if [ "$2" == "" ]; then
			printf 'TESTBENCH_ERROR: github issue URL must be given\n'
			error_exit 100
		fi
		export TEST_STATUS="$1"
		export TEST_GITHUB_ISSUE="$2"
	else
		printf 'TESTBENCH_ERROR: test_status "%s" unknown\n' "$1"
		error_exit 100
	fi
}


setvar_RS_HOSTNAME() {
	printf '### Obtaining HOSTNAME (prerequisite, not actual test) ###\n'
	generate_conf ""
	add_conf 'module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="0" listenPortFileName="'$RSYSLOG_DYNNAME'.tcpflood_port")

$template hostname,"%hostname%"
local0.* ./'${RSYSLOG_DYNNAME}'.HOSTNAME;hostname
'
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	startup ""
	tcpflood -m1 -M "\"<128>\""
	shutdown_when_empty
	wait_shutdown ""
	export RS_HOSTNAME="$(cat ${RSYSLOG_DYNNAME}.HOSTNAME)"
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	echo HOSTNAME is: $RS_HOSTNAME
}


# begin a new testconfig
#	2018-09-07:	Incremented inputs.timeout.shutdown to 60000 because kafka tests may not be 
#			finished under stress otherwise
# $1 is the instance id, if given
generate_conf() {
	if [ "$RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT" == "" ]; then
		RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT="10000"
	fi
	if [ "$RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT" == "" ]; then
		RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT="60000"
	fi
	if [ "$RSTB_ACTION_DEFAULT_Q_TO_SHUTDOWN" == "" ]; then
		RSTB_ACTION_DEFAULT_Q_TO_SHUTDOWN="20000"
	fi
	if [ "$RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE" == "" ]; then
		RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE="20000"
	fi
	export TCPFLOOD_PORT="$(get_free_port)"
	if [ "$1" == "" ]; then
		export TESTCONF_NM="${RSYSLOG_DYNNAME}_" # this basename is also used by instance 2!
		export RSYSLOG_OUT_LOG="${RSYSLOG_DYNNAME}.out.log"
		export RSYSLOG2_OUT_LOG="${RSYSLOG_DYNNAME}_2.out.log"
		export RSYSLOG_PIDBASE="${RSYSLOG_DYNNAME}:" # also used by instance 2!
		mkdir $RSYSLOG_DYNNAME.spool
	fi
	echo 'module(load="../plugins/imdiag/.libs/imdiag")
global(inputs.timeout.shutdown="'$RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT'"
       default.action.queue.timeoutshutdown="'$RSTB_ACTION_DEFAULT_Q_TO_SHUTDOWN'"
       default.action.queue.timeoutEnqueue="'$RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE'")
# use legacy-style for the following settings so that we can override if needed
$MainmsgQueueTimeoutEnqueue 20000
$MainmsgQueueTimeoutShutdown '$RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT'
$IMDiagListenPortFileName '$RSYSLOG_DYNNAME.imdiag$1.port'
$IMDiagServerRun 0
$IMDiagAbortTimeout '$TB_TEST_MAX_RUNTIME'

:syslogtag, contains, "rsyslogd"  ./'${RSYSLOG_DYNNAME}$1'.started
###### end of testbench instrumentation part, test conf follows:' > ${TESTCONF_NM}$1.conf
}

# add more data to config file. Note: generate_conf must have been called
# $1 is config fragment, $2 the instance id, if given
add_conf() {
	printf '%s' "$1" >> ${TESTCONF_NM}$2.conf
}


rst_msleep() {
	$TESTTOOL_DIR/msleep $1
}


# compare file to expected exact content
# $1 is file to compare, default $RSYSLOG_OUT_LOG
cmp_exact() {
	filename=${1:-"$RSYSLOG_OUT_LOG"}
	if [ "$filename" == "" ]; then
		printf 'Testbench ERROR, cmp_exact() does not have a filename at ALL!\n'
		error_exit 100
	fi
	if [ "$EXPECTED" == "" ]; then
		printf 'Testbench ERROR, cmp_exact() needs to have env var EXPECTED set!\n'
		error_exit 100
	fi
	printf '%s\n' "$EXPECTED" | cmp - "$filename"
	if [ $? -ne 0 ]; then
		printf 'invalid response generated\n'
		printf '################# %s is:\n' "$filename"
		cat -n $filename
		printf '################# EXPECTED was:\n'
		cat -n <<< "$EXPECTED"
		printf '\n#################### diff is:\n'
		diff - "$filename" <<< "$EXPECTED"
		error_exit  1
	fi;
}

# code common to all startup...() functions
startup_common() {
	instance=
	if [ "$1" == "2" ]; then
	    CONF_FILE="${TESTCONF_NM}2.conf"
	    instance=2
	elif [ "$1" == "" ] || [ "$1" == "1" ]; then
	    CONF_FILE="${TESTCONF_NM}.conf"
	else
	    CONF_FILE="$srcdir/testsuites/$1"
	    instance=$2
	fi
	# we need to remove the imdiag port file as there are some
	# tests that start multiple times. These may get the old port
	# number if the file still exists AND timing is bad so that
	# imdiag does not generate the port file quickly enough on
	# startup.
	rm -f $RSYSLOG_DYNNAME.imdiag$instance.port
	if [ ! -f $CONF_FILE ]; then
	    echo "ERROR: config file '$CONF_FILE' not found!"
	    error_exit 1
	fi
	echo config $CONF_FILE is:
	cat -n $CONF_FILE
}

# wait for appearance of a specific pid file, given as $1
wait_startup_pid() {
	if [ "$1" == "" ]; then
		echo "FAIL: testbench bug: wait_startup_called without \$1"
		error_exit 100
	fi
	while test ! -f $1; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
		   printf '%s ABORT! Timeout waiting on startup (pid file %s)\n' "$(tb_timestamp)" "$1"
		   ls -l "$1"
		   ps -fp $($SUDO cat "$1")
		   error_exit 1
		fi
	done
	printf '%s %s found, pid %s\n' "$(tb_timestamp)" "$1" "$(cat $1)"
}

# special version of wait_startup_pid() for rsyslog startup
wait_rsyslog_startup_pid() {
	wait_startup_pid $RSYSLOG_PIDBASE$1.pid
}

# wait for startup of an arbitrary process
# $1 - pid file name
# $2 - startup file name (optional, only checked if given)
wait_process_startup() {
	wait_startup_pid $1.pid
	i=0
	if [ "$2" != "" ]; then
		while test ! -f "$2"; do
			$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
			ps -p $(cat $1.pid) &> /dev/null
			if [ $? -ne 0 ]
			then
			   echo "ABORT! pid in $1 no longer active during startup!"
			   error_exit 1
			fi
			(( i++ ))
			if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
			   printf '%s ABORT! Timeout waiting on file %s\n' "$(tb_timestamp)" "$2"
			   error_exit 1
			fi
		done
		printf '%s %s seen, associated pid %s\n' "$(tb_timestamp)" "$2" "$(cat $1)"
	fi
}


# wait for the pid in $1 to terminate, abort on timeout
wait_pid_termination() {
		out_pid="$1"
		if [[ "$out_pid" == "" ]]; then
			printf 'TESTBENCH error: pidfile name not specified in wait_pid_termination\n'
			error_exit 100
		fi
		terminated=0
		while [[ $terminated -eq 0 ]]; do
			ps -p $out_pid &> /dev/null
			if [[ $? != 0 ]]; then
				terminated=1
			fi
			$TESTTOOL_DIR/msleep 100
			if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
			   printf '%s ABORT! Timeout waiting on shutdown (pid %s)\n' "$(tb_timestamp)" $out_pid
			   ps -fp $out_pid
			   printf 'Instance is possibly still running and may need manual cleanup.\n'
			   error_exit 1
			fi
		done
		unset terminated
		unset out_pid
}

# wait for file $1 to exist AND be non-empty
# $1 : file to wait for
# $2 (optional): error message to show if timeout occurs
wait_file_exists() {
	echo waiting for file $1
	i=0
	while true; do
		if [ -f $1 ] && [ "$(cat $1 2> /dev/null)" != "" ]; then
			break
		fi
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		((i++))
		if test $i -gt $TB_TIMEOUT_STARTSTOP; then
		   echo "ABORT! Timeout waiting for file $1"
		   ls -l $1
		   if [ "$2" != "" ]; then
			echo "$2"
		   fi
		   error_exit 1
		fi
	done
}

# kafka special wait function: we wait for the output file in order
# to ensure Kafka/Zookeeper is actually ready to go. This is NOT
# a generic check function and must only used with those kafka tests
# that actually need it.
kafka_wait_group_coordinator() {
echo We are waiting for kafka/zookeeper being ready to deliver messages
wait_file_exists $RSYSLOG_OUT_LOG "

Non-existence of $RSYSLOG_OUT_LOG can be caused
by a problem inside zookeeper. If debug output in the receiver is enabled, one
may see this message:

\"GroupCoordinator response error: Broker: Group coordinator not available\"

In this case you may want to do a web search and/or have a look at
    https://github.com/edenhill/librdkafka/issues/799

The question, of course, is if there is nevertheless a problem in imkafka.
Usually, the wait we do inside the testbench is sufficient to handle all
Zookeeper/Kafka startup. So if the issue reoccurs, it is suggested to enable
debug output in the receiver and check for actual problems.
"
}

# check if kafka itself failed. $1 is the message file name.
kafka_check_broken_broker() {
	failed=0
	if grep "Broker transport failure" < "$1" ; then
		failed=1
	fi
	if grep "broker connections are down" < "$1" ; then
		failed=1
	fi
	if [ $failed -eq 1 ]; then
		printf '\n\nenvironment-induced test error - kafka broker failed - skipping test\n'
		printf 'content of %s:\n' "$1"
		cat -n "$1"
		error_exit 177
	fi
}

# inject messages via kafkacat tool (for imkafka tests)
# $1 == "--wait" means wait for rsyslog to receive TESTMESSAGES lines in RSYSLOG_OUT_LOG
# $TESTMESSAGES contains number of messages to inject
# $RANDTOPIC contains topic to produce to
injectmsg_kafkacat() {
	if [ "$1" == "--wait" ]; then
		wait="YES"
		shift
	fi
	if [ "$TESTMESSAGES" == "" ]; then
		printf 'TESTBENCH ERROR: TESTMESSAGES env var not set!\n'
		error_exit 1
	fi
	MAXATONCE=25000 # how many msgs should kafkacat send? - hint: current version errs out above ~70000
	i=1
	while (( i<=TESTMESSAGES )); do
		currmsgs=0
		while ((i <= $TESTMESSAGES && currmsgs != MAXATONCE)); do
			printf ' msgnum:%8.8d\n' $i;
			i=$((i + 1))
			currmsgs=$((currmsgs+1))
		done  > "$RSYSLOG_DYNNAME.kafkacat.in"
		set -e
		kafkacat -P -b localhost:29092 -t $RANDTOPIC <"$RSYSLOG_DYNNAME.kafkacat.in" 2>&1 | tee >$RSYSLOG_DYNNAME.kafkacat.log
		set +e
		printf 'kafkacat injected %d msgs so far\n' $((i - 1))
		kafka_check_broken_broker $RSYSLOG_DYNNAME.kafkacat.log
		check_not_present "ERROR" $RSYSLOG_DYNNAME.kafkacat.log
		cat $RSYSLOG_DYNNAME.kafkacat.log
	done

	if [ "$wait" == "YES" ]; then
		wait_seq_check "$@"
	fi
}


# wait for rsyslogd startup ($1 is the instance)
wait_startup() {
	wait_rsyslog_startup_pid $1
	while test ! -f ${RSYSLOG_DYNNAME}$1.started; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		ps -p $(cat $RSYSLOG_PIDBASE$1.pid) &> /dev/null
		if [ $? -ne 0 ]
		then
		   echo "ABORT! rsyslog pid no longer active during startup!"
		   error_exit 1 stacktrace
		fi
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
		   printf '%s ABORT! Timeout waiting startup file %s\n' "$(tb_timestamp)" "${RSYSLOG_DYNNAME}.started"
		   error_exit 1
		fi
	done
	echo "$(tb_timestamp) rsyslogd$1 startup msg seen, pid " $(cat $RSYSLOG_PIDBASE$1.pid)
	wait_file_exists $RSYSLOG_DYNNAME.imdiag$1.port
	eval export IMDIAG_PORT$1=$(cat $RSYSLOG_DYNNAME.imdiag$1.port)
	eval PORT='$IMDIAG_PORT'$1
	echo "imdiag$1 port: $PORT"
	if [ "$PORT" == "" ]; then
		echo "TESTBENCH ERROR: imdiag port not found!"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
	fi
}

# reassign ports after rsyslog startup; must be called from all
# functions that startup rsyslog
reassign_ports() {
	if grep -q 'listenPortFileName="'$RSYSLOG_DYNNAME'\.tcpflood_port"' $CONF_FILE; then
		assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
	fi
	if grep -q '$InputTCPServerListenPortFile.*\.tcpflood_port' $CONF_FILE; then
		assign_tcpflood_port $RSYSLOG_DYNNAME.tcpflood_port
	fi
}

# start rsyslogd with default params. $1 is the config file name to use
# returns only after successful startup, $2 is the instance (blank or 2!)
# RS_REDIR maybe set to redirect rsyslog output
# env var RSTB_DAEMONIZE" == "YES" means rsyslogd shall daemonize itself;
# any other value or unset means it does not do that.
startup() {
	if [ "$USE_VALGRIND" == "YES" ]; then
		startup_vg "$1" "$2"
		return
	elif [ "$USE_VALGRIND" == "YES-NOLEAK" ]; then
		startup_vg_noleak "$1" "$2"
		return
	fi
	startup_common "$1" "$2"
	if [ "$RSTB_DAEMONIZE" == "YES" ]; then
		n_option=""
	else
		n_option="-n"
	fi
	eval LD_PRELOAD=$RSYSLOG_PRELOAD $valgrind ../tools/rsyslogd -C $n_option -i$RSYSLOG_PIDBASE$instance.pid -M../runtime/.libs:../.libs -f$CONF_FILE $RS_REDIR &
	wait_startup $instance
	reassign_ports
}


# assign TCPFLOOD_PORT from port file
# $1 - port file
assign_tcpflood_port() {
	wait_file_exists "$1"
	export TCPFLOOD_PORT=$(cat "$1")
	echo "TCPFLOOD_PORT now: $TCPFLOOD_PORT"
	if [ "$TCPFLOOD_PORT" == "" ]; then
		echo "TESTBENCH ERROR: TCPFLOOD_PORT not found!"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
	fi
}


# assign TCPFLOOD_PORT2 from port file
# $1 - port file
assign_tcpflood_port2() {
	wait_file_exists "$1"
	export TCPFLOOD_PORT2=$(cat "$1")
	echo "TCPFLOOD_PORT2 now: $TCPFLOOD_PORT2"
	if [ "$TCPFLOOD_PORT2" == "" ]; then
		echo "TESTBENCH ERROR: TCPFLOOD_PORT2 not found!"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
	fi
}
# assign RS_PORT from port file - this is meant as generic way to
# obtain additional port variables
# $1 - port file
assign_rs_port() {
	wait_file_exists "$1"
	export RS_PORT=$(cat "$1")
	echo "RS_PORT now: $RS_PORT"
	if [ "$RS_PORT" == "" ]; then
		echo "TESTBENCH ERROR: RS_PORT not found!"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
	fi
}

# wait for a file to exist, then export it's content to env var
# intended to be used for very small files, e.g. listenPort files
# $1 - env var name
# $2 - port file
assign_file_content() {
	wait_file_exists "$2"
	content=$(cat "$2")
	if [ "$content" == "" ]; then
		echo "TESTBENCH ERROR: get_file content had empty file $2"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
	fi
	eval export $1="$content"
	printf 'exported: %s=%s\n' $1 "$content"
}

# same as startup_vg, BUT we do NOT wait on the startup message!
startup_vg_waitpid_only() {
	startup_common "$1" "$2"
	if [ "$RS_TESTBENCH_LEAK_CHECK" == "" ]; then
		RS_TESTBENCH_LEAK_CHECK=full
	fi
	# add --keep-debuginfo=yes for hard to find cases; this cannot be used generally,
	# because it is only supported by newer versions of valgrind (else CI will fail
	# on older platforms).
	LD_PRELOAD=$RSYSLOG_PRELOAD valgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --gen-suppressions=all --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=$RS_TESTBENCH_LEAK_CHECK ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$instance.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
	wait_rsyslog_startup_pid $1
}

# start rsyslogd with default params under valgrind control. $1 is the config file name to use
# returns only after successful startup, $2 is the instance (blank or 2!)
startup_vg() {
		startup_vg_waitpid_only $1 $2
		wait_startup $instance
		reassign_ports
}

# same as startup-vg, except that --leak-check is set to "none". This
# is meant to be used in cases where we have to deal with libraries (and such
# that) we don't can influence and where we cannot provide suppressions as
# they are platform-dependent. In that case, we can't test for leak checks
# (obviously), but we can check for access violations, what still is useful.
startup_vg_noleak() {
	RS_TESTBENCH_LEAK_CHECK=no
	startup_vg "$@"
}

# same as startup-vgthread, BUT we do NOT wait on the startup message!
startup_vgthread_waitpid_only() {
	startup_common "$1" "$2"
	valgrind --tool=helgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --suppressions=$srcdir/linux_localtime_r.supp --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --suppressions=$srcdir/CI/gcov.supp --gen-suppressions=all ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$2.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
	wait_rsyslog_startup_pid $2
}

# start rsyslogd with default params under valgrind thread debugger control.
# $1 is the config file name to use, $2 is the instance (blank or 2!)
# returns only after successful startup
startup_vgthread() {
	startup_vgthread_waitpid_only $1 $2
	wait_startup $2
	reassign_ports
}


# inject messages via our inject interface (imdiag)
# $1 is start message number, env var NUMMESSAGES is number of messages to inject
injectmsg() {
	if [ "$3" != "" ] ; then
		printf 'error: injectmsg only has two arguments, extra arg is %s\n' "$3"
	fi
	msgs=${2:-$NUMMESSAGES}
	echo injecting $msgs messages
	echo injectmsg "${1:-0}" "$msgs" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
}

# inject messages in INSTANCE 2 via our inject interface (imdiag)
injectmsg2() {
	msgs=${2:-$NUMMESSAGES}
	echo injecting $msgs messages into instance 2
	echo injectmsg "${1:-0}" "$msgs" $3 $4 | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
	# TODO: some return state checking? (does it really make sense here?)
}

# inject literal payload  via our inject interface (imdiag)
injectmsg_literal() {
	printf 'injecting msg payload: %s\n' "$1"
	sed -e 's/^/injectmsg literal /g' <<< "$1" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit $?
}

# inject literal payload  via our inject interface (imdiag)
injectmsg_file() {
	printf 'injecting msg payload: %s\n' "$1"
	sed -e 's/^/injectmsg literal /g' < "$1" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit $?
}


# show the current main queue size. $1 is the instance.
get_mainqueuesize() {
	if [ "$1" == "2" ]; then
		echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
	else
		echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
	fi
}

# get pid of rsyslog instance $1
getpid() {
		printf '%s' "$(cat $RSYSLOG_PIDBASE$1.pid)"
}

# grep for (partial) content. $1 is the content to check for, $2 the file to check
# option --check-only just returns success/fail but does not terminate on fail
#    this is meant for checking during queue shutdown processing.
# option --regex is understood, in which case $1 is a regex
content_check() {
	if [ "$1" == "--check-only" ]; then
		check_only="yes"
		shift
	else
		check_only="no"
	fi
	if [ "$1" == "--regex" ]; then
		grep_opt=
		shift
	else
		grep_opt=-F
	fi
	if [ "$1" == "--output-results" ]; then
		output_results="yes"
		shift
	else
		output_results="no"
	fi
	file=${2:-$RSYSLOG_OUT_LOG}
	if ! grep -q  $grep_opt -- "$1" < "${file}"; then
	    if [ "$check_only" == "yes" ]; then
		printf 'content_check did not yet succeed\n'
		return 1
	    fi
	    printf '\n============================================================\n'
	    printf 'FILE "%s" content:\n' "$file"
	    cat -n ${file}
	    printf 'FAIL: content_check failed to find "%s"\n' "$1"
	    error_exit 1
	else
	    if [ "$output_results" == "yes" ]; then
		# Output GREP results
		echo "SUCCESS: content_check found results for '$1'\n"
		grep "$1" "${file}"
	    fi
	fi
	if [ "$check_only" == "yes" ]; then
	    return 0
	fi
}


# grep for (partial) content. this checks the count of the content
# $1 is the content to check for
# $2 required count
# $3 the file to check (if default not used)
# option --regex is understood, in which case $1 is a regex
content_count_check() {
	if [ "$1" == "--regex" ]; then
		grep_opt=
		shift
	else
		grep_opt=-F
	fi
	file=${3:-$RSYSLOG_OUT_LOG}
	count=$(grep -c $grep_opt -- "$1" <${RSYSLOG_OUT_LOG})
	if [ ${count:=0} -ne "$2" ]; then
	    grep -c -F -- "$1" <${RSYSLOG_OUT_LOG}
	    printf '\n============================================================\n'
	    printf 'FILE "%s" content:\n' "$file"
	    cat -n ${file}
	    printf 'FAIL: content count required %d but was %d\n' "$2" $count
	    printf 'FAIL: content_check failed to find "%s"\n' "$1"
	    error_exit 1
	fi
}



# $1 - content to check for
# $2 - number of times content must appear
# $3 - timeout (default: 1)
content_check_with_count() {
	timeoutend=${3:-1}
	timecounter=0
	while [  $timecounter -lt $timeoutend ]; do
		(( timecounter=timecounter+1 ))
		count=0
		if [ -f "${RSYSLOG_OUT_LOG}" ]; then
			count=$(grep -c -F -- "$1" <${RSYSLOG_OUT_LOG})
		fi
		if [ ${count:=0} -eq $2 ]; then
			echo content_check_with_count SUCCESS, \"$1\" occurred $2 times
			break
		else
			if [ "$timecounter" == "$timeoutend" ]; then
				shutdown_when_empty ""
				wait_shutdown ""

				echo "$(tb_timestamp)" content_check_with_count failed, expected \"$1\" to occur $2 times, but found it "$count" times
				echo file $RSYSLOG_OUT_LOG content is:
				if [ $(wc -l < "$RSYSLOG_OUT_LOG") -gt 10000 ]; then
					printf 'truncation, we have %d lines, which is way too much\n' \
						$(wc -l < "$RSYSLOG_OUT_LOG")
					printf 'showing first and last 5000 lines\n'
					head -n 5000 < "$RSYSLOG_OUT_LOG"
					print '\n ... CUT ..................................................\n\n'
					tail -n 5000 < "$RSYSLOG_OUT_LOG"
				else
					cat -n "$RSYSLOG_OUT_LOG"
				fi
				error_exit 1
			else
				printf '%s content_check_with_count, try %d have %d, wait for %d, search for: "%s"\n' \
					"$(tb_timestamp)" "$timecounter" "$count" "$2" "$1"
				$TESTTOOL_DIR/msleep 1000
			fi
		fi
	printf '%s **** content_check_with_count DEBUG (timeout %s, need %s lines):\n' "$(tb_timestamp)" "$3"  "$2" # rger: REMOVE ME when problems are fixed
	if [ -f "${RSYSLOG_OUT_LOG}" ]; then cat -n "$RSYSLOG_OUT_LOG"; fi
	done
}


custom_content_check() {
	grep -qF -- "$1" < $2
	if [ "$?" -ne "0" ]; then
	    echo FAIL: custom_content_check failed to find "'$1'" inside "'$2'"
	    echo "file contents:"
	    cat -n $2
	    error_exit 1
	fi
}

# check that given content $1 is not present in file $2 (default: RSYSLOG_OUT_LOG)
# regular expressions may be used
check_not_present() {
	if [ "$2" == "" ]; then
		file=$RSYSLOG_OUT_LOG
	else
		file="$2"
	fi
	grep -q -- "$1" < "$file"
	if [ "$?" -eq "0" ]; then
		echo FAIL: check_not present found
		echo $1
		echo inside file $file of $(wc -l < $file) lines
		echo samples:
		cat -n "$file" | grep -- "$1" | head -10
		error_exit 1
	fi
}


# check if mainqueue spool files exist, if not abort (we just check .qi).
check_mainq_spool() {
	printf 'There must exist some files now:\n'
	ls -l $RSYSLOG_DYNNAME.spool
	printf '.qi file:\n'
	cat $RSYSLOG_DYNNAME.spool/mainq.qi
	if [ ! -f $RSYSLOG_DYNNAME.spool/mainq.qi ]; then
		printf 'error: mainq.qi does not exist where expected to do so!\n'
		error_exit 1
	fi
}
# check that no spool file exists. Abort if they do.
# This situation must exist after a successful termination of rsyslog
# where the disk queue has properly been drained and shut down.
check_spool_empty() {
	if [ "$(ls $RSYSLOG_DYNNAME.spool/* 2> /dev/null)" != "" ]; then
		printf 'error: spool files exists where they are not permitted to do so:\n'
		ls -l $RSYSLOG_DYNNAME.spool/*
		error_exit 1
	fi
}

# general helper for imjournal tests: check that we got hold of the
# injected test message. This is pretty lengthy as the journal has played
# "a bit" with us and also seems to sometimes have a heavy latency in
# forwarding messages. So let's centralize the check code.
#
# $TESTMSG must contain the test message
check_journal_testmsg_received() {
	printf 'checking that journal indeed contains test message - may take a short while...\n'
	# search reverse, gets us to our message (much) faster .... if it is there...
	journalctl -a -r | grep -qF "$TESTMSG"
	if [ $? -ne 0 ]; then
		print 'SKIP: cannot read journal - our testmessage not found via journalctl\n'
		exit 77
	fi
	printf 'journal contains test message\n'

	echo "INFO: $(wc -l < $RSYSLOG_OUT_LOG) lines in $RSYSLOG_OUT_LOG"

	grep -qF "$TESTMSG" < $RSYSLOG_OUT_LOG
	if [ $? -ne 0 ]; then
	  echo "FAIL:  $RSYSLOG_OUT_LOG content (tail -n200):"
	  tail -n200 $RSYSLOG_OUT_LOG
	  echo "======="
	  echo "searching journal for testbench messages:"
	  journalctl -a | grep -qF "TestBenCH-RSYSLog imjournal"
	  echo "======="
	  echo "NOTE: showing only last 200 lines, may be insufficient on busy systems!"
	  echo "last entries from journal:"
	  journalctl -an 200
	  echo "======="
	  echo "NOTE: showing only last 200 lines, may be insufficient on busy systems!"
	  echo "However, the test check all of the journal, we are just limiting the output"
	  echo "to 200 lines to not spam CI systems too much."
	  echo "======="
	  echo "FAIL: imjournal test message could not be found!"
	  echo "Expected message content was:"
	  echo "$TESTMSG"
	  error_exit 1
	fi;
}

# checks that among the open files found in /proc/<PID>/fd/*
# there is or is not, depending on the calling mode,
# a link with the specified suffix in the target name
check_fd_for_pid() {
  local pid="$1" mode="$2" suffix="$3" target seen
  seen="false"
  for fd in $(echo /proc/$pid/fd/*); do
    target="$(readlink -m "$fd")"
    if [[ "$target" != *$RSYSLOG_DYNNAME* ]]; then
      continue
    fi
    if ((i % 10 == 0)); then
      echo "INFO: check target='$target'"
    fi
    if [[ "$target" == *$suffix ]]; then
      seen="true"
      if [[ "$mode" == "exists" ]]; then
        echo "PASS: check fd for pid=$pid mode='$mode' suffix='$suffix'"
        return 0
      fi
    fi
  done
  if [[ "$seen" == "false" ]] && [[ "$mode" == "absent" ]]; then
    echo "PASS: check fd for pid=$pid mode='$mode' suffix='$suffix'"
    return 0
  fi
  echo "FAIL: check fd for pid=$pid mode='$mode' suffix='$suffix'"
  if [[ "$mode" != "ignore" ]]; then
    return 1
  fi
  return 0
}

# wait for main message queue to be empty. $1 is the instance.
# we run in a loop to ensure rsyslog is *really* finished when a
# function for the "finished predicate" is defined. This is done
# by setting env var QUEUE_EMPTY_CHECK_FUNC to the bash
# function name.
wait_queueempty() {
	while [ $(date +%s) -le $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; do
		if [ "$1" == "2" ]; then
			echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
		else
			echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		fi
		if [ "$QUEUE_EMPTY_CHECK_FUNC" == "" ]; then
			return
		else
			if $QUEUE_EMPTY_CHECK_FUNC ; then
				return
			fi
		fi
	done
	error_exit $TB_ERR_TIMEOUT
}


# shut rsyslogd down when main queue is empty. $1 is the instance.
shutdown_when_empty() {
	echo Shutting down instance ${1:-1}
	wait_queueempty $1
	if [ "$RSYSLOG_PIDBASE" == "" ]; then
		echo "RSYSLOG_PIDBASE is EMPTY! - bug in test? (instance $1)"
		error_exit 1
	fi
	cp $RSYSLOG_PIDBASE$1.pid $RSYSLOG_PIDBASE$1.pid.save
	$TESTTOOL_DIR/msleep 500 # wait a bit (think about slow testbench machines!)
	kill $(cat $RSYSLOG_PIDBASE$1.pid) # note: we do not wait for the actual termination!
}

# shut rsyslogd down without emptying the queue. $2 is the instance.
shutdown_immediate() {
	pidfile=$RSYSLOG_PIDBASE${1:-}.pid
	cp $pidfile $pidfile.save
	kill $(cat $pidfile)
}


# actually, we wait for rsyslog.pid to be deleted.
# $1 is the instance
wait_shutdown() {
	if [ "$USE_VALGRIND" == "YES" ] || [ "$USE_VALGRIND" == "YES-NOLEAK" ]; then
		wait_shutdown_vg "$1"
		return
	fi
	out_pid=$(cat $RSYSLOG_PIDBASE$1.pid.save)
	printf '%s wait on shutdown of %s\n' "$(tb_timestamp)" "$out_pid"
	if [[ "$out_pid" == "" ]]
	then
		terminated=1
	else
		terminated=0
	fi
	while [[ $terminated -eq 0 ]]; do
		ps -p $out_pid &> /dev/null
		if [[ $? != 0 ]]; then
			terminated=1
		fi
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
		   printf '%s wait_shutdown ABORT! Timeout waiting on shutdown (pid %s)\n' "$(tb_timestamp)" $out_pid
		   ps -fp $out_pid
		   echo "Instance is possibly still running and may need"
		   echo "manual cleanup."
		   echo "TRYING TO capture status via gdb from hanging process"
		   $SUDO gdb ../tools/rsyslogd <<< "attach $out_pid
set pagination off
inf thr
thread apply all bt
quit"
		   echo "trying to kill -9 process"
		   kill -9 $out_pid
		   error_exit 1
		fi
	done
	unset terminated
	unset out_pid
	if [ "$(ls core.* 2>/dev/null)" != "" ]; then
	   printf 'ABORT! core file exists (maybe from a parallel run!)\n'
	   pwd
	   ls -l core.*
	   error_exit  1
	fi
}


# wait for all pending lookup table reloads to complete $1 is the instance.
await_lookup_table_reload() {
	if [ "$1" == "2" ]; then
		echo AwaitLookupTableReload | $TESTTOOL_DIR/diagtalker -pIMDIAG_PORT2 || error_exit  $?
	else
		echo AwaitLookupTableReload | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
	fi
}

# $1 filename, default $RSYSLOG_OUT_LOG
# $2 expected nbr of lines, default $NUMMESSAGES
# $3 timeout in seconds
# options (need to be specified in THIS ORDER if multiple given):
# --delay ms              -- if given, delay to use between retries
# --abort-on-oversize     -- error_exit if more lines than expected are present
# --count-function func   -- function to call to obtain current count
#                    this permits to override the default predicate and makes
#                    the wait really universally usable.
wait_file_lines() {
	delay=200
	if [ "$1" == "--delay" ]; then
		delay="$2"
		shift 2
	fi
	abort_oversize=NO
	if [ "$1" == "--abort-on-oversize" ]; then
		abort_oversize="YES"
		shift
	fi
	count_function=
	if [ "$1" == "--count-function" ]; then
		count_function="$2"
		shift 2
	fi
	timeout=${3:-$TB_TEST_TIMEOUT}
	timeoutbegin=$(date +%s)
	timeoutend=$(( timeoutbegin + timeout ))
	# TODO: change this to support odl mode, if needed: timeoutend=${3:-200}
	file=${1:-$RSYSLOG_OUT_LOG}
	waitlines=${2:-$NUMMESSAGES}

	while true ; do
		count=0
		if [ "$count_function" == "" ]; then
			if [ -f "$file" ]; then
				if [ "$COUNT_FILE_IS_ZIPPED" == "yes" ]; then
					issue_HUP ""
					count=$(gunzip < "$file" | wc -l)
				else
					count=$(wc -l < "$file")
				fi
			fi
		else
			count=$($count_function)
		fi
		if [ ${count} -gt $waitlines ]; then
			if [ $abort_oversize == "YES" ] && [ ${count} -gt $waitlines ]; then
				printf 'FAIL: wait_file_lines, too many lines, expected %d, current %s, took %s seconds\n' \
					$waitlines $count "$(( $(date +%s) - timeoutbegin ))"
				error_exit 1
			else
				printf 'wait_file_lines success, target %d or more lines, have %d, took %d seconds\n' \
					"$waitlines" $count "$(( $(date +%s) - timeoutbegin ))"
				return
			fi
		fi
		if [ ${count} -eq $waitlines ]; then
			echo wait_file_lines success, have $waitlines lines, took $(( $(date +%s) - timeoutbegin )) seconds, file "$file"
			break
		else
			if [ $(date +%s) -ge $timeoutend  ]; then
				echo wait_file_lines failed, expected $waitlines got $count after $timeoutend retries, took $(( $(date +%s) - timeoutbegin )) seconds
				error_exit 1
			else
				echo $(date +%H:%M:%S) wait_file_lines waiting, expected $waitlines, current $count lines
				$TESTTOOL_DIR/msleep $delay
			fi
		fi
	done
}




# wait until seq_check succeeds. This is used to synchronize various
# testbench timing-related issues, most importantly rsyslog shutdown
# all parameters are passed to seq_check
wait_seq_check() {
	timeoutend=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	if [ "$SEQ_CHECK_FILE" == "" ]; then
		filename="$RSYSLOG_OUT_LOG"
	else
		filename="$SEQ_CHECK_FILE"
	fi

	while true ; do
		if [ "$PRE_SEQ_CHECK_FUNC" != "" ]; then
			$PRE_SEQ_CHECK_FUNC
		fi
		if [ "${filename##.*}" != "gz" ]; then
			if [ -f "$filename" ]; then
				count=$(wc -l < "$filename")
			fi
		fi
		seq_check --check-only "$@" #&>/dev/null
		ret=$?
		if [ $ret == 0 ]; then
			printf 'wait_seq_check success (%d lines)\n' "$count"
			break
		else
			if [ $(date +%s) -ge $timeoutend  ]; then
				printf 'wait_seq_check failed, no success before timeout\n'
				error_exit 1
			else
				printf 'wait_seq_check waiting (%d lines)\n' $count
				$TESTTOOL_DIR/msleep 500
			fi
		fi
	done
	unset count
}


# wait until some content appears in the specified file.
# This is used to synchronize various
# testbench timing-related issues, most importantly rsyslog shutdown
# all parameters are passed to seq_check
# $1 - content to search for
# $2 - file to check
wait_content() {
	file=${2:-$RSYSLOG_OUT_LOG}
	timeoutend=$(( $(date +%s) + TB_TEST_TIMEOUT ))
	count=0

	while true ; do
		if [ -f "$file" ]; then
			count=$(wc -l < "$file")
			if grep -q "$1" < "$file"; then
				printf 'expected content found, continue test (%d lines)\n' "$count"
				break
			fi
		fi
		if [ $(date +%s) -ge $timeoutend  ]; then
			printf 'wait_content failed, no success before timeout (%d lines)\n' "$count"
			printf 'searched content was:\n%s\n' "$1"
			error_exit 1
		else
			printf 'wait_content still waiting... (%d lines)\n' "$count"
			tail "$file"
			$TESTTOOL_DIR/msleep 500
		fi
	done
	unset count
}


assert_content_missing() {
	grep -qF -- "$1" < ${RSYSLOG_OUT_LOG}
	if [ "$?" -eq "0" ]; then
		echo content-missing assertion failed, some line matched pattern "'$1'"
		error_exit 1
	fi
}


custom_assert_content_missing() {
	grep -qF -- "$1" < $2
	if [ "$?" -eq "0" ]; then
		echo content-missing assertion failed, some line in "'$2'" matched pattern "'$1'"
		cat -n "$2"
		error_exit 1
	fi
}


# shut rsyslogd down when main queue is empty. $1 is the instance.
issue_HUP() {
	if [ "$1" == "--sleep" ]; then
		sleeptime="$2"
		shift 2
	else
		sleeptime=1000
	fi
	kill -HUP $(cat $RSYSLOG_PIDBASE$1.pid)
	printf 'HUP issued to pid %d\n' $(cat $RSYSLOG_PIDBASE$1.pid)
	$TESTTOOL_DIR/msleep $sleeptime
}


# actually, we wait for rsyslog.pid to be deleted. $1 is the instance
wait_shutdown_vg() {
	wait $(cat $RSYSLOG_PIDBASE$1.pid)
	export RSYSLOGD_EXIT=$?
	echo rsyslogd run exited with $RSYSLOGD_EXIT

	if [ "$(ls vgcore.* 2>/dev/null)" != "" ]; then
	   printf 'ABORT! core file exists:\n'
	   ls -l vgcore.*
	   error_exit 1
	fi
	if [ "$USE_VALGRIND" == "YES" ] || [ "$USE_VALGRIND" == "YES-NOLEAK" ]; then
		check_exit_vg
	fi
}

check_file_exists() {
	if [ ! -f "$1" ]; then
		printf 'FAIL: file "%s" must exist, but does not\n' "$1"
		error_exit 1
	fi
}

check_file_not_exists() {
	if [ -f "$1" ]; then
		printf 'FILE %s CONTENT:\n' "$1"
		cat -n -- "$1"
		printf 'FAIL: file "%s" must NOT exist, but does\n' "$1"
		error_exit 1
	fi
}

# check exit code for valgrind error
check_exit_vg(){
	if [ "$RSYSLOGD_EXIT" -eq "10" ]; then
		printf 'valgrind run FAILED with exceptions - terminating\n'
		error_exit 1
	fi
}


# do cleanup during exit processing
do_cleanup() {
	if [ "$(type -t test_error_exit_handler)" == "function" ]; then
		test_error_exit_handler
	fi

	if [ -f $RSYSLOG_PIDBASE.pid ]; then
		printf 'rsyslog pid file still exists, trying to shutdown...\n'
		shutdown_immediate ""
	fi
	if [ -f ${RSYSLOG_PIDBASE}1.pid ]; then
		printf 'rsyslog pid file still exists, trying to shutdown...\n'
		shutdown_immediate 1
	fi
	if [ -f ${RSYSLOG_PIDBASE}2.pid ]; then
		printf 'rsyslog pid file still exists, trying to shutdown...\n'
		shutdown_immediate 2
	fi
}


# this is called if we had an error and need to abort. Here, we
# try to gather as much information as possible. That's most important
# for systems like Travis-CI where we cannot debug on the machine itself.
# our $1 is the to-be-used exit code. if $2 is "stacktrace", call gdb.
#
# NOTE: if a function test_error_exit_handler is defined, error_exit will
#       call it immediately before termination. This may be used to cleanup
#       some things or emit additional diagnostic information.
error_exit() {
	if [ $1 -eq $TB_ERR_TIMEOUT ]; then
		printf '%s Test %s exceeded max runtime of %d seconds\n' "$(tb_timestamp)" "$0" $TB_TEST_MAX_RUNTIME
	fi
	if [ "$(ls core* 2>/dev/null)" != "" ]; then
		echo trying to obtain crash location info
		echo note: this may not be the correct file, check it
		CORE=$(ls core*)
		echo "bt" >> gdb.in
		echo "q" >> gdb.in
		gdb ../tools/rsyslogd $CORE -batch -x gdb.in
		CORE=
		rm gdb.in
	fi
	if [[ "$2" == 'stacktrace' || ( ! -e IN_AUTO_DEBUG &&  "$USE_AUTO_DEBUG" == 'on' ) ]]; then
		if [ "$(ls core* 2>/dev/null)" != "" ]; then
			CORE=$(ls core*)
			printf 'trying to analyze core "%s" for main rsyslogd binary\n' "$CORE"
			printf 'note: this may not be the correct file, check it\n'
			$SUDO gdb ../tools/rsyslogd "$CORE" -batch <<- EOF
				bt
				echo === THREAD INFO ===
				info thread
				echo === thread apply all bt full ===
				thread apply all bt full
				q
				EOF
			$SUDO gdb ./tcpflood "$CORE" -batch <<- EOF
				bt
				echo === THREAD INFO ===
				info thread
				echo === thread apply all bt full ===
				thread apply all bt full
				q
				EOF
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
	# output listening ports as a temporary debug measure (2018-09-08 rgerhards), now disables, but not yet removed (2018-10-22)
	#if [ $(uname) == "Linux" ]; then
	#	netstat -tlp
	#else
	#	netstat
	#fi

	# Extended debug output for dependencies started by testbench
	if [ "$EXTRA_EXITCHECK" == 'dumpkafkalogs' ] && [ "$TEST_OUTPUT" == "VERBOSE" ]; then
		# Dump Zookeeper log
		dump_zookeeper_serverlog
		# Dump Kafka log
		dump_kafka_serverlog
	fi

	# Extended Exit handling for kafka / zookeeper instances 
	kafka_exit_handling "false"

	# Ensure redis instance is stopped
	if [ -n "$REDIS_DYN_DIR" ]; then
		stop_redis
	fi

	error_stats $1 # Report error to rsyslog testbench stats
	do_cleanup

	exitval=$1
	if [ "$TEST_STATUS" == "unreliable" ] && [ "$1" -ne 100 ]; then
		# TODO: log github issue
		printf 'Test flagged as unreliable, exiting with SKIP. Original exit code was %d\n' "$1"
		printf 'GitHub ISSUE: %s\n' "$TEST_GITHUB_ISSUE"
		exitval=77
	else
		if [ "$1" -eq 177 ]; then
			exitval=77
		fi
	fi
	printf '%s FAIL: Test %s (took %s seconds)\n' "$(tb_timestamp)" "$0" "$(( $(date +%s) - TB_STARTTEST ))"
	if [ $exitval -ne 77 ]; then
		echo $0 > testbench_test_failed_rsyslog
	fi
	exit $exitval
}


# skip a test; do cleanup when we detect it is necessary
skip_test(){
	do_cleanup
	exit 77
}


# Helper function to call rsyslog project test error script
# $1 is the exit code
error_stats() {
	if [ "$RSYSLOG_STATSURL" == "" ]; then
		printf 'not reporting failure as RSYSLOG_STATSURL is not set\n'
	else
		echo reporting failure to $RSYSLOG_STATSURL
		testname=$($PYTHON $srcdir/urlencode.py "$RSYSLOG_TESTNAME")
		testenv=$($PYTHON $srcdir/urlencode.py "${VCS_SLUG:-$PWD}")
		testmachine=$($PYTHON $srcdir/urlencode.py "$HOSTNAME")
		logurl=$($PYTHON $srcdir/urlencode.py "${CI_BUILD_URL:-}")
		wget -nv -O/dev/null $RSYSLOG_STATSURL\?Testname=$testname\&Testenv=$testenv\&Testmachine=$testmachine\&exitcode=${1:-1}\&logurl=$logurl\&rndstr=jnxv8i34u78fg23
	fi
}

# do the usual sequence check to see if everything was properly received.
# $4... are just to have the ability to pass in more options...
# add -v to chkseq if you need more verbose output
# argument --check-only can be used to simply do a check without abort in fail case
# env var SEQ_CHECK_FILE permits to override file name to check
# env var SEQ_CHECK_OPTIONS provide the ability to add extra options for check program
seq_check() {
	if [ "$SEQ_CHECK_FILE" == "" ]; then
		SEQ_CHECK_FILE="$RSYSLOG_OUT_LOG"
	fi
	if [ "$1" == "--check-only" ]; then
		check_only="YES"
		shift
	else
		check_only="NO"
	fi
	if [ "$1" == "" ]; then
		if [ "$NUMMESSAGES" == "" ]; then
			printf 'FAIL: seq_check called without parameters but NUMMESSAGES is unset!\n'
			error_exit 100
		fi
		# use default parameters
		startnum=0
		endnum=$(( NUMMESSAGES - 1 ))
	else
		startnum=$1
		endnum=$2
	fi
	if [ ! -f "$SEQ_CHECK_FILE" ]; then
		if [ "$check_only"  == "YES" ]; then
			return 1
		fi
		printf 'FAIL: %s does not exist in seq_check!\n' "$SEQ_CHECK_FILE"
		error_exit 1
	fi
	if [ "${SEQ_CHECK_FILE##*.}" == "gz" ]; then
		gunzip -c "${SEQ_CHECK_FILE}" | $RS_SORTCMD $RS_SORT_NUMERIC_OPT | ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7 $SEQ_CHECK_OPTIONS
	elif [ "${SEQ_CHECK_FILE##*.}" == "zst" ]; then
		ls -l "${SEQ_CHECK_FILE}"
		unzstd < "${SEQ_CHECK_FILE}" | $RS_SORTCMD $RS_SORT_NUMERIC_OPT | ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7 $SEQ_CHECK_OPTIONS
	else
		$RS_SORTCMD $RS_SORT_NUMERIC_OPT < "${SEQ_CHECK_FILE}" | ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7 $SEQ_CHECK_OPTIONS
	fi
	ret=$?
	if [ "$check_only"  == "YES" ]; then
		return $ret
	fi
	if [ $ret -ne 0 ]; then
		if [ "${SEQ_CHECK_FILE##*.}" == "gz" ]; then
			gunzip -c "${SEQ_CHECK_FILE}" | $RS_SORTCMD $RS_SORT_NUMERIC_OPT \
				| ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7 $SEQ_CHECK_OPTIONS \
				> $RSYSLOG_DYNNAME.error.log
		elif [ "${SEQ_CHECK_FILE##*.}" == "zst" ]; then
			unzstd < "${SEQ_CHECK_FILE}" | $RS_SORTCMD $RS_SORT_NUMERIC_OPT \
				| ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7 $SEQ_CHECK_OPTIONS \
				> $RSYSLOG_DYNNAME.error.log
		else
			$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${SEQ_CHECK_FILE} \
				> $RSYSLOG_DYNNAME.error.log
		fi
		echo "sequence error detected in $SEQ_CHECK_FILE"
		echo "number of lines in file: $(wc -l $SEQ_CHECK_FILE)"
		echo "sorted data has been placed in error.log, first 10 lines are:"
		cat -n "$RSYSLOG_DYNNAME.error.log" | head -10
		echo "---last 10 lines are:"
		cat -n "$RSYSLOG_DYNNAME.error.log" | tail -10
		echo "UNSORTED data, first 10 lines are:"
		cat -n "$RSYSLOG_DYNNAME.error.log" | head -10
		echo "---last 10 lines are:"
		cat -n "$RSYSLOG_DYNNAME.error.log" | tail -10
		# for interactive testing, create a static filename. We know this may get
		# mangled during a parallel test run
		mv -f $RSYSLOG_DYNNAME.error.log error.log
		error_exit 1 
	fi
	return 0
}


# do the usual sequence check to see if everything was properly received. This is
# a duplicateof seq-check, but we could not change its calling conventions without
# breaking a lot of exitings test cases, so we preferred to duplicate the code here.
# $4... are just to have the ability to pass in more options...
# add -v to chkseq if you need more verbose output
seq_check2() {
	$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${RSYSLOG2_OUT_LOG}  | ./chkseq -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do the usual sequence check, but for gzip files
# $4... are just to have the ability to pass in more options...
gzip_seq_check() {
	if [ "$1" == "" ]; then
		if [ "$NUMMESSAGES" == "" ]; then
			printf 'FAIL: gzip_seq_check called without parameters but NUMMESSAGES is unset!\n'
			error_exit 100
		fi
		# use default parameters
		startnum=0
		endnum=$(( NUMMESSAGES - 1 ))
	else
		startnum=$1
		endnum=$2
	fi
	ls -l ${RSYSLOG_OUT_LOG}
	gunzip < ${RSYSLOG_OUT_LOG} | $RS_SORTCMD $RS_SORT_NUMERIC_OPT | ./chkseq -v -s$startnum -e$endnum $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do a tcpflood run and check if it worked params are passed to tcpflood
tcpflood() {
	if [ "$1" == "--check-only" ]; then
		check_only="yes"
		shift
	else
		check_only="no"
	fi
	eval ./tcpflood -p$TCPFLOOD_PORT "$@" $TCPFLOOD_EXTRA_OPTS
	res=$?
	if [ "$check_only" == "yes" ]; then
		if [ "$res" -ne "0" ]; then
			echo "error during tcpflood on port ${TCPFLOOD_PORT}! But test continues..."
		fi
		return 0
	else
		if [ "$res" -ne "0" ]; then
			echo "error during tcpflood on port ${TCPFLOOD_PORT}! see ${RSYSLOG_OUT_LOG}.save for what was written"
			cp ${RSYSLOG_OUT_LOG} ${RSYSLOG_OUT_LOG}.save
			error_exit 1 stacktrace
		fi
	fi
}


# cleanup
# detect any left-over hanging instance
exit_test() {
	nhanging=0
	#for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
	#do
		#echo "ERROR: left-over instance $pid, killing it"
	#	ps -fp $pid
	#	pwd
	#	printf "we do NOT kill the instance as this does not work with multiple\n"
	#	printf "builds per machine - this message is now informational to show prob exists!\n"
	#	#kill -9 $pid
	#	let "nhanging++"
	#done
	if test $nhanging -ne 0
	then
	   echo "ABORT! hanging instances left at exit"
	   #error_exit 1
	   #exit 77 # for now, just skip - TODO: reconsider when supporting -j
	fi
	# now real cleanup
	rm -f rsyslog.action.*.include
	rm -f work rsyslog.out.* xlate*.lkp_tbl
	rm -rf test-logdir stat-file1
	rm -f rsyslog.conf.tlscert stat-file1 rsyslog.empty imfile-state:*
	rm -f ${TESTCONF_NM}.conf
	rm -f tmp.qi nocert
	rm -fr $RSYSLOG_DYNNAME*  # delete all of our dynamic files
	unset TCPFLOOD_EXTRA_OPTS

	# Extended Exit handling for kafka / zookeeper instances 
	kafka_exit_handling "true"

	# Ensure redis is stopped
	stop_redis

	printf '%s Test %s SUCCESSFUL (took %s seconds)\n' "$(tb_timestamp)" "$0" "$(( $(date +%s) - TB_STARTTEST ))"
	echo  -------------------------------------------------------------------------------
	exit 0
}

# finds a free port that we can bind a listener to
# Obviously, any solution is race as another process could start
# just after us and grab the same port. However, in practice it seems
# to work pretty well. In any case, we should probably call this as
# late as possible before the usage of the port.
get_free_port() {
$PYTHON -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'
}


# return the inode number of file $1; file MUST exist
get_inode() {
	if [ ! -f "$1" ]; then
		printf 'FAIL: file "%s" does not exist in get_inode\n' "$1"
		error_exit 100
	fi
	$PYTHON -c 'import os; import stat; print(os.lstat("'$1'")[stat.ST_INO])'
}


# check that logger supports -d option, if not skip test
# right now this is a bit dirty, we check distros which do not support it
check_logger_has_option_d() {
	skip_platform "FreeBSD"  "We need logger -d option, which we do not have on FreeBSD"
	skip_platform "SunOS"  "We need logger -d option, which we do not have on (all flavors of) Solaris"

	# check also the case for busybox
	logger --help 2>&1 | head -n1 | grep -q BusyBox
	if [ $? -eq 0 ]; then
		echo "We need logger -d option, which we do not have have on Busybox"
		exit 77
	fi
}


require_relpEngineSetTLSLibByName() {
	./have_relpEngineSetTLSLibByName
	if [ $? -eq 1 ]; then
	  echo "relpEngineSetTLSLibByName API not available. Test stopped"
	  exit 77
	fi;
}

require_relpEngineVersion() {
	if [ "$1" == "" ]; then
		  echo "require_relpEngineVersion missing required parameter  (minimum version required)"
		  exit 1
	else
		./check_relpEngineVersion $1
		if [ $? -eq 1 ]; then
		  echo "relpEngineVersion too OLD. Test stopped"
		  exit 77
		fi;
	fi
}


# check if command $1 is available - will exit 77 when not OK
check_command_available() {
	have_cmd=0
	if [ "$1" == "timeout" ]; then
		if timeout --version &>/dev/null ; then
			have_cmd=1
		fi
	else
		command -v $1
		if [ $? -eq 0 ]; then
			have_cmd=1
		fi
	fi
	if [ $have_cmd -eq 0 ] ; then
		printf 'Testbench requires unavailable command: %s\n' "$1"
		exit 77 # do NOT error_exit here!
	fi
}


# sort the output file just like we normally do it, but do not call
# seqchk. This is needed for some operations where we need the sort
# result for some preprocessing. Note that a later seqchk will sort
# again, but that's not an issue.
presort() {
	rm -f $RSYSLOG_DYNNAME.presort
	$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${RSYSLOG_OUT_LOG} > $RSYSLOG_DYNNAME.presort
}


#START: ext kafka config
#dep_cache_dir=$(readlink -f .dep_cache)
export RS_ZK_DOWNLOAD=apache-zookeeper-3.8.2-bin.tar.gz
dep_cache_dir=$(pwd)/.dep_cache
dep_zk_url=https://downloads.apache.org/zookeeper/zookeeper-3.8.2/$RS_ZK_DOWNLOAD
dep_zk_cached_file=$dep_cache_dir/$RS_ZK_DOWNLOAD

export RS_KAFKA_DOWNLOAD=kafka_2.13-2.8.0.tgz
dep_kafka_url="https://www.rsyslog.com/files/download/rsyslog/$RS_KAFKA_DOWNLOAD"
dep_kafka_cached_file=$dep_cache_dir/$RS_KAFKA_DOWNLOAD

if [ -z "$ES_DOWNLOAD" ]; then
	export ES_DOWNLOAD=elasticsearch-7.14.1-linux-x86_64.tar.gz
fi
if [ -z "$ES_PORT" ]; then
	export ES_PORT=19200
fi
dep_es_cached_file="$dep_cache_dir/$ES_DOWNLOAD"

# kafka (including Zookeeper)
dep_kafka_dir_xform_pattern='s#^[^/]\+#kafka#g'
dep_zk_dir_xform_pattern='s#^[^/]\+#zk#g'
dep_es_dir_xform_pattern='s#^[^/]\+#es#g'
#dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka.log)

#	TODO Make dynamic work dir for multiple instances
#dep_work_dir=$(readlink -f .dep_wrk)
dep_work_dir=$(pwd)/.dep_wrk
#dep_kafka_work_dir=$dep_work_dir/kafka
#dep_zk_work_dir=$dep_work_dir/zk

#END: ext kafka config

kafka_exit_handling() {

	# Extended Exit handling for kafka / zookeeper instances 
	if [[ "$EXTRA_EXIT" == 'kafka' ]]; then
		
		echo "stop kafka instance"
		stop_kafka '.dep_wrk' $1

		echo "stop zookeeper instance"
		stop_zookeeper '.dep_wrk' $1
	fi

	# Extended Exit handling for kafka / zookeeper instances 
	if [[ "$EXTRA_EXIT" == 'kafkamulti' ]]; then
		echo "stop kafka instances"
		stop_kafka '.dep_wrk1' $1
		stop_kafka '.dep_wrk2' $1
		stop_kafka '.dep_wrk3' $1

		echo "stop zookeeper instances"
		stop_zookeeper '.dep_wrk1' $1
		stop_zookeeper '.dep_wrk2' $1
		stop_zookeeper '.dep_wrk3' $1
	fi
}

download_kafka() {
	if [ ! -d $dep_cache_dir ]; then
		echo "Creating dependency cache dir $dep_cache_dir"
		mkdir $dep_cache_dir
	fi
	if [ ! -f $dep_zk_cached_file ]; then
		if [ -f /local_dep_cache/$RS_ZK_DOWNLOAD ]; then
			printf 'Zookeeper: satisfying dependency %s from system cache.\n' "$RS_ZK_DOWNLOAD"
			cp /local_dep_cache/$RS_ZK_DOWNLOAD $dep_zk_cached_file
		else
			echo "Downloading zookeeper"
			echo wget -q $dep_zk_url -O $dep_zk_cached_file
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
	fi
	if [ ! -f $dep_kafka_cached_file ]; then
		if [ -f /local_dep_cache/$RS_KAFKA_DOWNLOAD ]; then
			printf 'Kafka: satisfying dependency %s from system cache.\n' "$RS_KAFKA_DOWNLOAD"
			cp /local_dep_cache/$RS_KAFKA_DOWNLOAD $dep_kafka_cached_file
		else
			echo "Downloading kafka"
			wget -q $dep_kafka_url -O $dep_kafka_cached_file
			if [ $? -ne 0 ]
			then
				echo error during wget, retry:
				wget $dep_kafka_url -O $dep_kafka_cached_file
				if [ $? -ne 0 ]
				then
					rm $dep_kafka_cached_file # a 0-size file may be left over
					error_exit 1
				fi
			fi
		fi
	fi
}

stop_kafka() {
	if [ "$KEEP_KAFKA_RUNNING" == "YES" ]; then
		return
	fi
	i=0
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_kafka_config="kafka-server.properties"
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
		if [[ ".dep_wrk" !=  "$1" ]]; then
			dep_work_kafka_config="kafka-server$1.properties"
		else
			dep_work_kafka_config="kafka-server.properties"
		fi
	fi
	if [ ! -d $dep_work_dir/kafka ]; then
		echo "Kafka work-dir $dep_work_dir/kafka does not exist, no action needed"
	else
		# shellcheck disable=SC2009  - we do not grep on the process name!
		kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')

		echo "Stopping Kafka instance $1 ($dep_work_kafka_config/$kafkapid)"
		kill $kafkapid

		# Check if kafka instance went down!
		while true; do
			# shellcheck disable=SC2009  - we do not grep on the process name!
			kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
			if [[ "" !=  "$kafkapid" ]]; then
				$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
				if test $i -gt $TB_TIMEOUT_STARTSTOP; then
					echo "Kafka instance $dep_work_kafka_config (PID $kafkapid) still running - Performing hard shutdown (-9)"
					kill -9 $kafkapid
					break
				fi
				(( i++ ))
			else
				# Break the loop
				break
			fi
		done
		
		if [[ "$2" == 'true' ]]; then
			# Process shutdown, do cleanup now
			cleanup_kafka $1
		fi
	fi
}

cleanup_kafka() {
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
	fi
	if [ ! -d $dep_work_dir/kafka ]; then
		echo "Kafka work-dir $dep_work_dir/kafka does not exist, no action needed"
	else
		echo "Cleanup Kafka instance $1"
		rm -rf $dep_work_dir/kafka
	fi
}

stop_zookeeper() {
	if [ "$KEEP_KAFKA_RUNNING" == "YES" ]; then
		return
	fi
	i=0
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_tk_config="zoo.cfg"
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
		if [[ ".dep_wrk" !=  "$1" ]]; then
			dep_work_tk_config="zoo$1.cfg"
		else
			dep_work_tk_config="zoo.cfg"
		fi
	fi

	if [ ! -d $dep_work_dir/zk ]; then
		echo "Zookeeper work-dir $dep_work_dir/zk does not exist, no action needed"
	else
		# Get Zookeeper pid instance
		zkpid=$(ps aux | grep -i $dep_work_tk_config | grep java | grep -v grep | awk '{print $2}')
		echo "Stopping Zookeeper instance $1 ($dep_work_tk_config/$zkpid)"
		kill $zkpid

		# Check if Zookeeper instance went down!
		zkpid=$(ps aux | grep -i $dep_work_tk_config | grep java | grep -v grep | awk '{print $2}')
		if [[ "" !=  "$zkpid" ]]; then
			while true; do
				zkpid=$(ps aux | grep -i $dep_work_tk_config | grep java | grep -v grep | awk '{print $2}')
				if [[ "" !=  "$zkpid" ]]; then
					$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
					if test $i -gt $TB_TIMEOUT_STARTSTOP; then
						echo "Zookeeper instance $dep_work_tk_config (PID $zkpid) still running - Performing hard shutdown (-9)"
						kill -9 $zkpid
						break
					fi
					(( i++ ))
				else
					break
				fi
			done
		fi

		if [[ "$2" == 'true' ]]; then
			# Process shutdown, do cleanup now
			cleanup_zookeeper $1
		fi
		rm "$ZOOPIDFILE"
	fi
}

cleanup_zookeeper() {
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
	fi
	rm -rf $dep_work_dir/zk
}

start_zookeeper() {
	if [ "$KEEP_KAFKA_RUNNING" == "YES" ] && [ -f "$ZOOPIDFILE" ]; then
		if kill -0 "$(cat "$ZOOPIDFILE")"; then
			printf 'zookeeper already running, no need to start\n'
			return
		else
			printf 'INFO: zookeeper pidfile %s exists, but zookeeper not running\n' "$ZOOPIDFILE"
			printf 'deleting pid file\n'
			rm -f "$ZOOPIDFILE"
		fi
	fi
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_tk_config="zoo.cfg"
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
		dep_work_tk_config="zoo$1.cfg"
	fi

	if [ ! -f $dep_zk_cached_file ]; then
			echo "Dependency-cache does not have zookeeper package, did you download dependencies?"
			error_exit 77
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
	echo "Starting Zookeeper instance $1"
	(cd $dep_work_dir/zk && ./bin/zkServer.sh start)
	wait_startup_pid "$ZOOPIDFILE"
}

start_kafka() {
	printf '%s starting kafka\n' "$(tb_timestamp)"

	# Force IPv4 usage of Kafka!
	export KAFKA_HEAP_OPTS="-Xms256m -Xmx256m" # we need to take care for smaller CI systems!
	export KAFKA_OPTS="-Djava.net.preferIPv4Stack=True"
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_kafka_config="kafka-server.properties"
	else
		dep_work_dir=$(readlink -f $1)
		dep_work_kafka_config="kafka-server$1.properties"
	fi

	# shellcheck disable=SC2009  - we do not grep on the process name!
	kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
	if [ "$KEEP_KAFKA_RUNNING" == "YES" ] && [ "$kafkapid" != "" ]; then
		printf 'kafka already running, no need to start\n'
		return
	fi

	if [ ! -f $dep_kafka_cached_file ]; then
			echo "Dependency-cache does not have kafka package, did you download dependencies?"
			error_exit 77
	fi
	if [ ! -d $dep_work_dir ]; then
			echo "Creating dependency working directory"
			mkdir -p $dep_work_dir
	fi
	rm -rf $dep_work_dir/kafka
	( cd $dep_work_dir && 
	  tar -zxvf $dep_kafka_cached_file --xform $dep_kafka_dir_xform_pattern --show-transformed-names) > /dev/null
	cp -f $srcdir/testsuites/$dep_work_kafka_config $dep_work_dir/kafka/config/
	#if [ "$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')" != "" ]; then
	echo "Starting Kafka instance $dep_work_kafka_config"
	(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
	$TESTTOOL_DIR/msleep 4000

	# Check if kafka instance came up!
	# shellcheck disable=SC2009  - we do not grep on the process name!
	kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
	if [[ "" !=  "$kafkapid" ]];
	then
		echo "Kafka instance $dep_work_kafka_config (PID $kafkapid) started ... "
	else
		echo "Starting Kafka instance $dep_work_kafka_config, SECOND ATTEMPT!"
		(cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)
		$TESTTOOL_DIR/msleep 4000

		# shellcheck disable=SC2009  - we do not grep on the process name!
		kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
		if [[ "" !=  "$kafkapid" ]];
		then
			echo "Kafka instance $dep_work_kafka_config (PID $kafkapid) started ... "
		else
			echo "Failed to start Kafka instance for $dep_work_kafka_config"
			echo "displaying all kafka logs now:"
			for logfile in $dep_work_dir/logs/*; do
				echo "FILE: $logfile"
				cat $logfile
			done
			error_exit 77
		fi
	fi
}

create_kafka_topic() {
	if [ "x$2" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $2)
	fi
	if [ "x$3" == "x" ]; then
		dep_work_port='2181'
	else
		dep_work_port=$3
	fi
	if [ ! -d $dep_work_dir/kafka ]; then
			echo "Kafka work-dir $dep_work_dir/kafka does not exist, did you start kafka?"
			exit 1
	fi
	if [ "x$1" == "x" ]; then
			echo "Topic-name not provided."
			exit 1
	fi

	# we need to make sure replication has is working. So let's loop until no more
	# errors (or timeout) - see also: https://github.com/rsyslog/rsyslog/issues/3045
	timeout_ready=100 # roughly 10 sec
	is_retry=0
	i=0
	while true; do
		text=$(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --zookeeper localhost:$dep_work_port/kafka --create --topic $1 --replication-factor 1 --partitions 2 )
		grep "Error.* larger than available brokers: 0" <<<"$text"
		res=$?
		if [ $res -ne 0 ]; then
			echo looks like brokers are available - continue...
			break
		fi
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		(( i++ ))
		if test $i -gt $timeout_ready; then
			echo "ENV ERROR: kafka brokers did not come up:"
			cat -n <<< $text
			if [ $is_retry == 1 ]; then
				echo "SKIPing test as the env is not ready for it"
				exit 177
			fi
			echo "RETRYING kafka startup, doing shutdown and startup"
			stop_zookeeper ""; stop_kafka ""
			start_zookeeper ""; start_kafka ""
			echo "READY for RETRY"
			is_retry=1
			i=0
		fi
	done

	# we *assume* now all goes well
	(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --zookeeper localhost:$dep_work_port/kafka --alter --topic $1 --delete-config retention.ms)
	(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --zookeeper localhost:$dep_work_port/kafka --alter --topic $1 --delete-config retention.bytes)
}

delete_kafka_topic() {
	if [ "x$2" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $srcdir/$2)
	fi
	if [ "x$3" == "x" ]; then
		dep_work_port='2181'
	else
		dep_work_port=$3
	fi

	echo "deleting kafka-topic $1"
	(cd $dep_work_dir/kafka && ./bin/kafka-topics.sh --delete --zookeeper localhost:$dep_work_port/kafka --topic $1)
}

dump_kafka_topic() {
	if [ "x$2" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka.log)
	else
		dep_work_dir=$(readlink -f $srcdir/$2)
		dep_kafka_log_dump=$(readlink -f rsyslog.out.kafka$2.log)
	fi
	if [ "x$3" == "x" ]; then
		dep_work_port='2181'
	else
		dep_work_port=$3
	fi

	echo "dumping kafka-topic $1"
	if [ ! -d $dep_work_dir/kafka ]; then
			echo "Kafka work-dir does not exist, did you start kafka?"
			exit 1
	fi
	if [ "x$1" == "x" ]; then
			echo "Topic-name not provided."
			exit 1
	fi

	(cd $dep_work_dir/kafka && ./bin/kafka-console-consumer.sh --timeout-ms 2000 --from-beginning --zookeeper localhost:$dep_work_port/kafka --topic $1 > $dep_kafka_log_dump)
}

dump_kafka_serverlog() {
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
	fi
	if [ ! -d $dep_work_dir/kafka ]; then
		echo "Kafka work-dir $dep_work_dir/kafka does not exist, no kafka debuglog"
	else
		echo "Dumping server.log from Kafka instance $1"
		echo "========================================="
		cat $dep_work_dir/kafka/logs/server.log
		echo "========================================="
		printf 'non-info is:\n'
		grep --invert-match '^\[.* INFO ' $dep_work_dir/kafka/logs/server.log | grep '^\['
	fi
}

dump_zookeeper_serverlog() {
	if [ "x$1" == "x" ]; then
		dep_work_dir=$(readlink -f .dep_wrk)
	else
		dep_work_dir=$(readlink -f $srcdir/$1)
	fi
	echo "Dumping zookeeper.out from Zookeeper instance $1"
	echo "========================================="
	cat $dep_work_dir/zk/zookeeper.out
	echo "========================================="
	printf 'non-info is:\n'
	grep --invert-match '^\[.* INFO ' $dep_work_dir/zk/zookeeper.out | grep '^\['
}


# download elasticsearch files, if necessary
download_elasticsearch() {
	if [ ! -d $dep_cache_dir ]; then
		echo "Creating dependency cache dir $dep_cache_dir"
		mkdir $dep_cache_dir
	fi
	if [ ! -f $dep_es_cached_file ]; then
		if [ -f /local_dep_cache/$ES_DOWNLOAD ]; then
			printf 'ElasticSearch: satisfying dependency %s from system cache.\n' "$ES_DOWNLOAD"
			cp /local_dep_cache/$ES_DOWNLOAD $dep_es_cached_file
		else
			dep_es_url="https://www.rsyslog.com/files/download/rsyslog/$ES_DOWNLOAD"
			printf 'ElasticSearch: satisfying dependency %s from %s\n' "$ES_DOWNLOAD" "$dep_es_url"
			wget -q $dep_es_url -O $dep_es_cached_file
		fi
	fi
}


# prepare elasticsearch execution environment
# this also stops any previous elasticsearch instance, if found
prepare_elasticsearch() {
	stop_elasticsearch # stop if it is still running
	# Heap Size (limit to 128MB for testbench! default is way to HIGH)
	export ES_JAVA_OPTS="-Xms128m -Xmx128m"

	dep_work_dir=$(readlink -f .dep_wrk)
	dep_work_es_config="es.yml"
	dep_work_es_pidfile="es.pid"

	if [ ! -f $dep_es_cached_file ]; then
		echo "Dependency-cache does not have elasticsearch package, did "
		echo "you download dependencies?"
		error_exit 100
	fi
	if [ ! -d $dep_work_dir ]; then
		echo "Creating dependency working directory"
		mkdir -p $dep_work_dir
	fi
	if [ -d $dep_work_dir/es ]; then
		if [ -e $dep_work_es_pidfile ]; then
			es_pid=$(cat $dep_work_es_pidfile)
			kill -SIGTERM $es_pid
			wait_pid_termination $es_pid
		fi
	fi
	rm -rf $dep_work_dir/es
	echo TEST USES ELASTICSEARCH BINARY $dep_es_cached_file
	(cd $dep_work_dir && tar -zxf $dep_es_cached_file --xform $dep_es_dir_xform_pattern --show-transformed-names) > /dev/null
	if [ -n "${ES_PORT:-}" ] ; then
		rm -f $dep_work_dir/es/config/elasticsearch.yml
		sed "s/^http.port:.*\$/http.port: ${ES_PORT}/" $srcdir/testsuites/$dep_work_es_config > $dep_work_dir/es/config/elasticsearch.yml
		if [ "$ES_DOWNLOAD" != "elasticsearch-6.0.0.tar.gz" ]; then
			printf 'xpack.security.enabled: false\n' >> $dep_work_dir/es/config/elasticsearch.yml
		fi
	else
		cp -f $srcdir/testsuites/$dep_work_es_config $dep_work_dir/es/config/elasticsearch.yml
	fi

	# Avoid deprecated parameter, new option introduced with 6.7
	echo "Setting transport tcp option to ${ES_PORT_OPTION:-transport.tcp.port}"
	sed -i "s/transport.tcp.port/${ES_PORT_OPTION:-transport.tcp.port}/g" "$dep_work_dir/es/config/elasticsearch.yml"

	if [ ! -d $dep_work_dir/es/data ]; then
			echo "Creating elastic search directories"
			mkdir -p $dep_work_dir/es/data
			mkdir -p $dep_work_dir/es/logs
			mkdir -p $dep_work_dir/es/tmp
	fi
	echo ElasticSearch prepared for use in test.
}


# ensure that a basic, suitable instance of elasticsearch is running. This is part
# of an effort to avoid restarting elasticsearch more often than necessary.
ensure_elasticsearch_ready() {
	if   printf '%s:%s:%s\n' "$ES_DOWNLOAD" "$ES_PORT" "$(cat es.pid)" \
	   | cmp -b - elasticsearch.running
	then
		printf 'Elasticsearch already running, NOT restarting it\n'
	else
		cat elasticsearch.running
		cleanup_elasticsearch
		dep_es_cached_file="$dep_cache_dir/$ES_DOWNLOAD"
		download_elasticsearch
		prepare_elasticsearch
		if [ "$1" != "--no-start" ]; then
			start_elasticsearch
			printf '%s:%s:%s\n' "$ES_DOWNLOAD" "$ES_PORT" "$(cat es.pid)" > elasticsearch.running
		fi
	fi
	if [ "$1" != "--no-start" ]; then
		printf 'running elasticsearch instance: %s\n' "$(cat elasticsearch.running)"
		init_elasticsearch
	fi
}


# $2, if set, is the number of additional ES instances
start_elasticsearch() {
	# Heap Size (limit to 256MB for testbench! defaults is way to HIGH)
	export ES_JAVA_OPTS="-Xms256m -Xmx256m"

	dep_work_dir=$(readlink -f .dep_wrk)
	dep_work_es_config="es.yml"
	dep_work_es_pidfile="$(pwd)/es.pid"
	echo "Starting ElasticSearch"

	# THIS IS THE ACTUAL START of ES
	$dep_work_dir/es/bin/elasticsearch -p $dep_work_es_pidfile -d
	$TESTTOOL_DIR/msleep 2000
	wait_startup_pid $dep_work_es_pidfile
	printf 'elasticsearch pid is %s\n' "$(cat $dep_work_es_pidfile)"

	# Wait for startup with hardcoded timeout
	timeoutend=120
	timeseconds=0
	# Loop until elasticsearch port is reachable or until
	# timeout is reached!
	until [ "$(curl --silent --show-error --connect-timeout 1 http://localhost:${ES_PORT:-19200} | grep 'rsyslog-testbench')" != "" ]; do
		echo "--- waiting for ES startup: $timeseconds seconds"
		$TESTTOOL_DIR/msleep 1000
		(( timeseconds=timeseconds + 1 ))

		if [ "$timeseconds" -gt "$timeoutend" ]; then 
			echo "--- TIMEOUT ( $timeseconds ) reached!!!"
			if [ ! -d $dep_work_dir/es ]; then
				echo "ElasticSearch $dep_work_dir/es does not exist, no ElasticSearch debuglog"
			else
				echo "Dumping rsyslog-testbench.log from ElasticSearch instance $1"
				echo "========================================="
				cat $dep_work_dir/es/logs/rsyslog-testbench.log
				echo "========================================="
#				printf 'non-info is:\n'
#				grep --invert-match '^\[.* INFO ' $dep_work_dir/kafka/logs/server.log | grep '^\['
			fi
			error_exit 1
		fi
	done
	$TESTTOOL_DIR/msleep 2000
	echo ES startup succeeded
}

# read data from ES to a local file so that we can process
# $1 - number of records (ES does not return all records unless you tell it explicitly).
# $2 - ES port
es_getdata() {
	curl --silent -XPUT --show-error -H 'Content-Type: application/json' "http://localhost:${2:-$ES_PORT}/rsyslog_testbench/_settings" -d '{ "index" : { "max_result_window" : '${1:-$NUMMESSAGES}' } }'
	# refresh to ensure we get the latest data
	curl --silent localhost:${2:-$ES_PORT}/rsyslog_testbench/_refresh
	curl --silent localhost:${2:-$ES_PORT}/rsyslog_testbench/_search?size=${1:-$NUMMESSAGES} > $RSYSLOG_DYNNAME.work
	$PYTHON $srcdir/es_response_get_msgnum.py > ${RSYSLOG_OUT_LOG}
}

# a standard method to support shutdown & queue empty check for a wide range
# of elasticsearch tests. This works if we assume that ES has delivered messages
# to the default location.
es_shutdown_empty_check() {
	es_getdata $NUMMESSAGES $ES_PORT
	lines=$(wc -l < "$RSYSLOG_OUT_LOG")
	if [ "$lines"  -eq $NUMMESSAGES ]; then
		printf '%s es_shutdown_empty_check: success, have %d lines\n' "$(tb_timestamp)" $lines
		return 0
	fi
	printf '%s es_shutdown_empty_check: have %d lines, expecting %d\n' "$(tb_timestamp)" $lines $NUMMESSAGES
	return 1
}


stop_elasticsearch() {
	dep_work_dir=$(readlink -f $srcdir)
	dep_work_es_pidfile="es.pid"
	rm elasticsearch.running
	if [ -e $dep_work_es_pidfile ]; then
		es_pid=$(cat $dep_work_es_pidfile)
		printf 'stopping ES with pid %d\n' $es_pid
		kill -SIGTERM $es_pid
		wait_pid_termination $es_pid
	fi
}

# cleanup es leftovers when it is being stopped
cleanup_elasticsearch() {
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_es_pidfile="es.pid"
		stop_elasticsearch
		rm -f $dep_work_es_pidfile
		rm -rf $dep_work_dir/es
}

# initialize local Elasticsearch *testbench* instance for the next
# test. NOTE: do NOT put anything useful on that instance!
init_elasticsearch() {
	curl --silent -XDELETE localhost:${ES_PORT:-9200}/rsyslog_testbench
}

omhttp_start_server() {
    # Args: 1=port 2=server args
    # Args 2 and up are passed along as is to omhttp_server.py
    omhttp_server_py=$srcdir/omhttp_server.py
    if [ ! -f $omhttp_server_py ]; then
        echo "Cannot find ${omhttp_server_py} for omhttp test"
        error_exit 1
    fi

    if [ "x$1" == "x" ]; then
        omhttp_server_port="8080"
    else
        omhttp_server_port="$1"
    fi

    # Create work directory for parallel tests
    omhttp_work_dir=$RSYSLOG_DYNNAME/omhttp

    omhttp_server_pidfile="${omhttp_work_dir}/omhttp_server.pid"
    omhttp_server_logfile="${omhttp_work_dir}/omhttp_server.log"
    mkdir -p ${omhttp_work_dir}

    server_args="-p $omhttp_server_port ${*:2} --port-file $RSYSLOG_DYNNAME.omhttp_server_lstnport.file"

    timeout 30m $PYTHON ${omhttp_server_py} ${server_args} >> ${omhttp_server_logfile} 2>&1 &
    if [ ! $? -eq 0 ]; then
        echo "Failed to start omhttp test server."
        rm -rf $omhttp_work_dir
        error_exit 1
    fi
    omhttp_server_pid=$!

    wait_file_exists "$RSYSLOG_DYNNAME.omhttp_server_lstnport.file"
    omhttp_server_lstnport="$(cat $RSYSLOG_DYNNAME.omhttp_server_lstnport.file)"
    echo ${omhttp_server_pid} > ${omhttp_server_pidfile}
    echo "Started omhttp test server with args ${server_args} with pid ${omhttp_server_pid}, port {$omhttp_server_lstnport}"
}

omhttp_stop_server() {
    # Args: None
    omhttp_work_dir=$RSYSLOG_DYNNAME/omhttp
    if [ ! -d $omhttp_work_dir ]; then
        echo "omhttp server $omhttp_work_dir does not exist, no action needed"
    else
        echo "Stopping omhttp server"
        kill -9 $(cat ${omhttp_work_dir}/omhttp_server.pid) > /dev/null 2>&1
        rm -rf $omhttp_work_dir
    fi
}

omhttp_get_data() {
    # Args: 1=port 2=endpoint 3=batchformat(optional)
    if [ "x$1" == "x" ]; then
        omhttp_server_port=8080
    else
        omhttp_server_port=$1
    fi

    if [ "x$2" == "x" ]; then
        omhttp_path=""
    else
        omhttp_path=$2
    fi

    # The test server returns a json encoded array of strings containing whatever omhttp sent to it in each request
    python_init="import json, sys; dat = json.load(sys.stdin)"
    python_print="print('\n'.join(out))"
    if [ "x$3" == "x" ]; then
        # dat = ['{"msgnum":"1"}, '{"msgnum":"2"}', '{"msgnum":"3"}', '{"msgnum":"4"}']
        python_parse="$python_init; out = [json.loads(l)['msgnum'] for l in dat]; $python_print"
    else
       if [ "x$3" == "xjsonarray" ]; then
            # dat = ['[{"msgnum":"1"},{"msgnum":"2"}]', '[{"msgnum":"3"},{"msgnum":"4"}]']
            python_parse="$python_init; out = [l['msgnum'] for a in dat for l in json.loads(a)]; $python_print"
        elif [ "x$3" == "xnewline" ]; then
            # dat = ['{"msgnum":"1"}\n{"msgnum":"2"}', '{"msgnum":"3"}\n{"msgnum":"4"}']
            python_parse="$python_init; out = [json.loads(l)['msgnum'] for a in dat for l in a.split('\n')]; $python_print"
        elif [ "x$3" == "xkafkarest" ]; then
            # dat = ['{"records":[{"value":{"msgnum":"1"}},{"value":{"msgnum":"2"}}]}',
            #        '{"records":[{"value":{"msgnum":"3"}},{"value":{"msgnum":"4"}}]}']
            python_parse="$python_init; out = [l['value']['msgnum'] for a in dat for l in json.loads(a)['records']]; $python_print"
        elif [ "x$3" == "xlokirest" ]; then
            # dat = ['{"streams":[{"msgnum":"1"},{"msgnum":"2"}]}',
            #        '{"streams":[{"msgnum":"3"},{"msgnum":"4"}]}']
            python_parse="$python_init; out = [l['msgnum'] for a in dat for l in json.loads(a)['streams']]; $python_print"
        else
            # use newline parsing as default
            python_parse="$python_init; out = [json.loads(l)['msgnum'] for a in dat for l in a.split('\n')]; $python_print"
        fi

    fi
    
    omhttp_url="localhost:${omhttp_server_port}/${omhttp_path}"
    curl -s ${omhttp_url} \
        | $PYTHON -c "${python_parse}" | sort -n \
        > ${RSYSLOG_OUT_LOG}
}


# prepare MySQL for next test
# each test receives its own database so that we also can run in parallel
mysql_prep_for_test() {
	mysql -u rsyslog --password=testbench -e "CREATE DATABASE $RSYSLOG_DYNNAME; "
	mysql -u rsyslog --password=testbench --database $RSYSLOG_DYNNAME \
		-e "CREATE TABLE SystemEvents (ID int unsigned not null auto_increment primary key, CustomerID bigint,ReceivedAt datetime NULL,DeviceReportedTime datetime NULL,Facility smallint NULL,Priority smallint NULL,FromHost varchar(60) NULL,Message text,NTSeverity int NULL,Importance int NULL,EventSource varchar(60),EventUser varchar(60) NULL,EventCategory int NULL,EventID int NULL,EventBinaryData text NULL,MaxAvailable int NULL,CurrUsage int NULL,MinUsage int NULL,MaxUsage int NULL,InfoUnitID int NULL,SysLogTag varchar(60),EventLogType varchar(60),GenericFileName VarChar(60),SystemID int NULL); CREATE TABLE SystemEventsProperties (ID int unsigned not null auto_increment primary key,SystemEventID int NULL,ParamName varchar(255) NULL,ParamValue text NULL);"
	mysql --user=rsyslog --password=testbench --database $RSYSLOG_DYNNAME \
		-e "truncate table SystemEvents;"
	# TEST ONLY:
	#mysql -s --user=rsyslog --password=testbench --database $RSYSLOG_DYNNAME \
		#-e "select substring(Message,9,8) from SystemEvents;"
	# END TEST
	printf 'mysql ready for test, database: %s\n' $RSYSLOG_DYNNAME
}

# get data from mysql DB so that we can do seq_check on it.
mysql_get_data() {
	# note "-s" is required to suppress the select "field header"
	mysql -s --user=rsyslog --password=testbench --database $RSYSLOG_DYNNAME \
		-e "select substring(Message,9,8) from SystemEvents;" \
		> $RSYSLOG_OUT_LOG 2> "$RSYSLOG_DYNNAME.mysqlerr"
	grep -iv "Using a password on the command line interface can be insecure." < "$RSYSLOG_DYNNAME.mysqlerr"
}

# cleanup any temp data from mysql test
# if we do not do this, we may run out of disk space
# especially in container environment.
mysql_cleanup_test() {
	mysql --user=rsyslog --password=testbench -e "drop database $RSYSLOG_DYNNAME;" \
		2>&1 | grep -iv "Using a password on the command line interface can be insecure."
}


start_redis() {
	check_command_available redis-server

	export REDIS_DYN_CONF="${RSYSLOG_DYNNAME}.redis.conf"
	export REDIS_DYN_DIR="$(pwd)/${RSYSLOG_DYNNAME}-redis"

	# Only set a random port if not set (useful when Redis must be restarted during a test)
	if [ -z "$REDIS_RANDOM_PORT" ]; then
		export REDIS_RANDOM_PORT="$(get_free_port)"
	fi

	cp $srcdir/testsuites/redis.conf $REDIS_DYN_CONF
	mkdir -p $REDIS_DYN_DIR

	sed -itemp "s+<tmpdir>+${REDIS_DYN_DIR}+g" $REDIS_DYN_CONF
	sed -itemp "s+<rndport>+${REDIS_RANDOM_PORT}+g" $REDIS_DYN_CONF

	# Start the server
	echo "Starting redis with conf file $REDIS_DYN_CONF"
	redis-server $REDIS_DYN_CONF &
	$TESTTOOL_DIR/msleep 2000

	# Wait for Redis to be fully up
	timeoutend=10
	until nc -w1 -z 127.0.0.1 $REDIS_RANDOM_PORT; do
		echo "Waiting for Redis to start..."
		$TESTTOOL_DIR/msleep 1000
		(( timeseconds=timeseconds + 2 ))

		if [ "$timeseconds" -gt "$timeoutend" ]; then
			echo "--- TIMEOUT ( $timeseconds ) reached!!!"
			if [ ! -d ${REDIS_DYN_DIR}/redis.log ]; then
				echo "no Redis logs"
			else
				echo "Dumping ${REDIS_DYN_DIR}/redis.log"
				echo "========================================="
				cat ${REDIS_DYN_DIR}/redis.log
				echo "========================================="
			fi
			error_exit 1
		fi
	done
}

cleanup_redis() {
	if [ -d ${REDIS_DYN_DIR} ]; then
		rm -rf ${REDIS_DYN_DIR}
	fi
	if [ -f ${REDIS_DYN_CONF} ]; then
		rm -f ${REDIS_DYN_CONF}
	fi
}

stop_redis() {
	if [ -f "$REDIS_DYN_DIR/redis.pid" ]; then
		redispid=$(cat $REDIS_DYN_DIR/redis.pid)
		echo "Stopping Redis instance"
		kill $redispid

		i=0

		# Check if redis instance went down!
		while [ -f $REDIS_DYN_DIR/redis.pid ]; do
			redispid=$(cat $REDIS_DYN_DIR/redis.pid)
			if [[ "" !=  "$redispid" ]]; then
				$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
				if test $i -gt $TB_TIMEOUT_STARTSTOP; then
					echo "redis server (PID $redispid) still running - Performing hard shutdown (-9)"
					kill -9 $redispid
					break
				fi
				(( i++ ))
			else
				# Break the loop
				break
			fi
		done
	fi
}

redis_command() {
	check_command_available redis-cli

	if [ -z "$1" ]; then
		echo "redis_command: no command provided!"
		error_exit 1
	fi

	printf "$1\n" | redis-cli -p "$REDIS_RANDOM_PORT"
}

# $1 - replacement string
# $2 - start search string
# $3 - file name
# $4 - expected value
first_column_sum_check() {
	sum=$(grep "$2" < "$3" | sed -e "$1" | awk '{s+=$1} END {print s}')
	if [ "x${sum}" != "x$4" ]; then
	    printf '\n============================================================\n'
	    echo FAIL: sum of first column with edit-expr "'$1'" run over lines from file "'$3'" matched by "'$2'" equals "'$sum'" which is NOT equal to EXPECTED value of "'$4'"
	    echo "file contents:"
	    cat $3
	    error_exit 1
	fi
}

#
# Helper functions to start/stop python snmp trap receiver
#
snmp_start_trapreceiver() {
    # Args: 1=port 2=outputfilename
    # Args 2 and up are passed along as is to snmptrapreceiver.py
    snmptrapreceiver=$srcdir/snmptrapreceiver.py
    if [ ! -f ${snmptrapreceiver} ]; then
        echo "Cannot find ${snmptrapreceiver} for omsnmp test"
        error_exit 1
    fi

    if [ "x$1" == "x" ]; then
        snmp_server_port="10162"
    else
        snmp_server_port="$1"
    fi

    if [ "x$2" == "x" ]; then
        output_file="${RSYSLOG_DYNNAME}.snmp.out"
    else
        output_file="$2"
    fi

    # Create work directory for parallel tests
    snmp_work_dir=${RSYSLOG_DYNNAME}/snmptrapreceiver

    snmp_server_pidfile="${snmp_work_dir}/snmp_server.pid"
    snmp_server_logfile="${snmp_work_dir}/snmp_server.log"
    mkdir -p ${snmp_work_dir}

    server_args="${snmp_server_port} 127.0.0.1 ${output_file}"

    $PYTHON ${snmptrapreceiver} ${server_args} ${snmp_server_logfile} >> ${snmp_server_logfile} 2>&1 &
    if [ ! $? -eq 0 ]; then
        echo "Failed to start snmptrapreceiver."
        rm -rf ${snmp_work_dir}
        error_exit 1
    fi

    snmp_server_pid=$!
    echo ${snmp_server_pid} > ${snmp_server_pidfile}

    while test ! -s "${snmp_server_logfile}"; do
	$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
	if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
	   printf '%s ABORT! Timeout waiting on startup (pid file %s)\n' "$(tb_timestamp)" "$1"
	   ls -l "$1"
	   ps -fp $(cat "$1")
	   snmp_stop_trapreceiver
	   error_exit 1
#	else 
#	   echo "waiting...${snmp_server_logfile}..."
	fi
    done

    echo "Started snmptrapreceiver with args ${server_args} with pid ${snmp_server_pid}"
}

snmp_stop_trapreceiver() {
    # Args: None
    snmp_work_dir=${RSYSLOG_DYNNAME}/snmptrapreceiver
    if [ ! -d ${snmp_work_dir} ]; then
        echo "snmptrapreceiver server ${snmp_work_dir} does not exist, no action needed"
    else
        echo "Stopping snmptrapreceiver server"
        kill -9 $(cat ${snmp_work_dir}/snmp_server.pid) > /dev/null 2>&1
        # Done at testexit already!: rm -rf ${snmp_work_dir}
    fi
}

wait_for_stats_flush() {
	echo "will wait for stats push"
	emitmsg=0
	while [[ ! -f $1 ]]; do
		if [ $((++emitmsg % 10)) == 0 ]; then
			echo waiting for stats file "'$1'" to be created
		fi
		$TESTTOOL_DIR/msleep 100
	done
	prev_count=$(grep -c 'BEGIN$' <$1)
	new_count=$prev_count
	start_loop="$(date +%s)"
	emit_waiting=0
	while [[ "x$prev_count" == "x$new_count" ]]; do
		# busy spin, because it allows as close timing-coordination
		# in actual test run as possible
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_TEST_MAX_RUNTIME )) ]; then
		   printf '%s ABORT! Timeout waiting on stats push\n' "$(tb_timestamp)" "$1"
		   error_exit 1
		else
		   # waiting for 1000 is heuristically "sufficiently but not too
		   # frequent" enough
		   if [ $((++emit_waiting)) == 1000 ]; then
		      printf 'still waiting for stats push...\n'
		      emit_waiting=0
		   fi
		 fi
		new_count=$(grep -c 'BEGIN$' <"$1")
	done
	echo "stats push registered"
}

# Check file exists and is of a particular size
# $1 - file to check
# $2 - size to check
file_size_check() {
    local size=$(ls -l $1 | awk {'print $5'})
    if [ "${size}" != "${2}" ]; then
	printf 'File:[%s] has unexpected size. Expected:[%d], Size:[%d]\n', $1 $2 $size
        error_exit 1
    fi
    return 0
}

case $1 in
   'init')	$srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
		source set-envvars
		# for (solaris) load debugging, uncomment next 2 lines:
		#export LD_DEBUG=all
		#ldd ../tools/rsyslogd

		# environment debug
		#find / -name "librelp.so*"
		#ps -ef |grep syslog
		#netstat -a | grep LISTEN

		# cleanup of hanging instances from previous runs
		# practice has shown this is pretty useful!
		#for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		#do
			#echo "ERROR: left-over previous instance $pid, killing it"
			#ps -fp $pid
			#pwd
			#kill -9 $pid
		#done
		# end cleanup

		# some default names (later to be set in other parts, once we support fully
		# parallel tests)
		export RSYSLOG_DFLT_LOG_INTERNAL=1 # testbench needs internal messages logged internally!
		if [ -f testbench_test_failed_rsyslog ] && [ "$ABORT_ALL_ON_TEST_FAIL" == "YES" ]; then
			echo NOT RUNNING TEST as previous test $(cat testbench_test_failed_rsyslog) failed.
			exit 77
		fi
		if [ "$RSYSLOG_DYNNAME" != "" ]; then
			echo "FAIL: \$RSYSLOG_DYNNAME already set in init"
			echo "hint: was init accidentally called twice?"
			exit 2
		fi
		export RSYSLOG_DYNNAME="rstb_$(./test_id $(basename $0))$(tr -dc 'a-zA-Z0-9' < /dev/urandom | head --bytes 4)"
		export RSYSLOG_OUT_LOG="${RSYSLOG_DYNNAME}.out.log"
		export RSYSLOG2_OUT_LOG="${RSYSLOG_DYNNAME}_2.out.log"
		export RSYSLOG_PIDBASE="${RSYSLOG_DYNNAME}:" # also used by instance 2!
		#export IMDIAG_PORT=13500 DELETE ME
		#export IMDIAG_PORT2=13501 DELETE ME
		#export TCPFLOOD_PORT=13514 DELETE ME

		# Extra Variables for Test statistic reporting
		export RSYSLOG_TESTNAME=$(basename $0)

		# we create one file with the test name, so that we know what was
		# left over if "make distcheck" complains
		touch $RSYSLOG_DYNNAME-$(basename $0).test_id

		if [ -z $RS_SORTCMD ]; then
			RS_SORTCMD="sort"
		fi
		if [ -z $RS_SORT_NUMERIC_OPT ]; then
			if [ "$(uname)" == "AIX" ]; then
				RS_SORT_NUMERIC_OPT=-n
			else
				RS_SORT_NUMERIC_OPT=-g
			fi
		fi
		if [ -z $RS_CMPCMD ]; then
			RS_CMPCMD="cmp"
		fi
		if [ -z $RS_HEADCMD ]; then
			RS_HEADCMD="head"
		fi
		# we assume TZ is set, else most test will fail. So let's ensure
		# this really is the case
		if [ -z $TZ ]; then
			echo "testbench: TZ env var not set, setting it to UTC"
			export TZ=UTC
		fi
		ulimit -c unlimited  &> /dev/null # at least try to get core dumps
		export TB_STARTTEST=$(date +%s)
		printf '%s\n' '------------------------------------------------------------'
		printf '%s Test: %s\n' "$(tb_timestamp)" "$0"
		printf '%s\n' '------------------------------------------------------------'
		rm -f xlate*.lkp_tbl
		rm -f log log* # RSyslog debug output 
		rm -f work 
		rm -rf test-logdir stat-file1
		rm -f rsyslog.empty imfile-state:* omkafka-failed.data
		rm -f tmp.qi nocert
		rm -f core.* vgcore.* core*
		# Note: rsyslog.action.*.include must NOT be deleted, as it
		# is used to setup some parameters BEFORE calling init. This
		# happens in chained test scripts. Delete on exit is fine,
		# though.
		# note: TCPFLOOD_EXTRA_OPTS MUST NOT be unset in init, because
		# some tests need to set it BEFORE calling init to accommodate
		# their generic test drivers.
		if [ "$TCPFLOOD_EXTRA_OPTS" != '' ] ; then
		        echo TCPFLOOD_EXTRA_OPTS set: $TCPFLOOD_EXTRA_OPTS
                fi
		if [ "$USE_AUTO_DEBUG" != 'on' ] ; then
			rm -f IN_AUTO_DEBUG
                fi
		if [ -e IN_AUTO_DEBUG ]; then
			export valgrind="valgrind --malloc-fill=ff --free-fill=fe --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --log-fd=1"
		fi
		;;

   'check-ipv6-available')   # check if IPv6  - will exit 77 when not OK
		if ip address > /dev/null ; then
			cmd="ip address"
		else
			cmd="ifconfig -a"
		fi
		echo command used for ipv6 detection: $cmd
		$cmd | grep ::1 > /dev/null
		if [ $? -ne 0 ] ; then
			printf 'this test requires an active IPv6 stack, which we do not have here\n'
			error_exit 77
		fi
		;;
   'kill-immediate') # kill rsyslog unconditionally
		kill -9 $(cat $RSYSLOG_PIDBASE.pid)
		# note: we do not wait for the actual termination!
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
	 'wait-for-dyn-stats-reset')
		echo "will wait for dyn-stats-reset"
		while [[ ! -f $2 ]]; do
				echo waiting for stats file "'$2'" to be created
				$TESTTOOL_DIR/msleep 100
		done
		prev_purged=$(grep -F 'origin=dynstats' < $2 | grep -F "${3}.purge_triggered=" | sed -e 's/.\+.purge_triggered=//g' | awk '{s+=$1} END {print s}')
		new_purged=$prev_purged
		while [[ "x$prev_purged" == "x$new_purged" ]]; do
				new_purged=$(grep -F 'origin=dynstats' < "$2" | grep -F "${3}.purge_triggered=" | sed -e 's/.\+\.purge_triggered=//g' | awk '{s+=$1} END {print s}') # busy spin, because it allows as close timing-coordination in actual test run as possible
				$TESTTOOL_DIR/msleep 10
		done
		echo "dyn-stats reset for bucket ${3} registered"
		;;
   'assert-first-column-sum-greater-than') 
		sum=$(grep $3 <$4| sed -e $2 | awk '{s+=$1} END {print s}')
		if [ ! $sum -gt $5 ]; then
		    echo sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is smaller than expected lower-limit of "'$5'"
		    echo "file contents:"
		    cat $4
		    error_exit 1
		fi
		;;
   'content-pattern-check') 
		grep -q "$2" < ${RSYSLOG_OUT_LOG}
		if [ "$?" -ne "0" ]; then
		    echo content-check failed, not every line matched pattern "'$2'"
		    echo "file contents:"
		    cat -n $4
		    error_exit 1
		fi
		;;
   'require-journalctl')   # check if journalctl exists on the system
		if ! hash journalctl 2>/dev/null ; then
		    echo "journalctl command missing, skipping test"
		    exit 77
		fi
		;;
	'check-inotify') # Check for inotify/fen support 
		if [ -n "$(find /usr/include -name 'inotify.h' -print -quit)" ]; then
			echo [inotify mode]
		elif [ -n "$(find /usr/include/sys/ -name 'port.h' -print -quit)" ]; then
			grep -qF "PORT_SOURCE_FILE" < /usr/include/sys/port.h
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
   *)		echo "TESTBENCH error: invalid argument" $1
		exit 100
esac
