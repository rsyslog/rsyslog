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
#
#
# EXIT STATES
# 0 - ok
# 1 - FAIL
# 77 - SKIP
# 100 - Testbench failure
# 177 - internal state: test failed, but in a way that makes us strongly believe
#       this is caused by environment. This will lead to exit 77 (SKIP), but report
#       the failure if failure reporting is active

# environment variables:
# USE_AUTO_DEBUG "on" --> enables automatic debugging, anything else
#                turns it off

# diag system internal environment variables
# these variables are for use by test scripts - they CANNOT be
# overriden by the user
# TCPFLOOD_EXTRA_OPTS   enables to set extra options for tcpflood, usually
#                       used in tests that have a common driver where it
#                       is too hard to set these options otherwise
# CONFIG
export ZOOPIDFILE="$(pwd)/zookeeper.pid"

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
TB_TEST_TIMEOUT=60  # number of seconds after which test checks timeout (eg. waits)
# note that 40sec for the startup should be sufficient even on very slow machines. we changed this from 2min on 2017-12-12
export RSYSLOG_DEBUG_TIMEOUTS_TO_STDERR="on"  # we want to know when we loose messages due to timeouts
if [ "$TESTTOOL_DIR" == "" ]; then
	export TESTTOOL_DIR="${srcdir:-.}"
fi

# newer functionality is preferrably introduced via bash functions
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
		printf 'platform is "%" - test does not work under "%s"\n' "$(uname $(uname))" "$1"
		if [ "$2" != "" ]; then
			printf 'reason: %s\n' "$2"
		fi
		exit 77
	fi

}


# a consistent format to output testbench timestamps
tb_timestamp() {
	date +%H:%M:%S
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
	printf '### Obtaining HOSTNAME (prequisite, not actual test) ###\n'
	generate_conf
	add_conf 'module(load="../plugins/imtcp/.libs/imtcp")
input(type="imtcp" port="'$TCPFLOOD_PORT'")

$template hostname,"%hostname%"
local0.* ./'${RSYSLOG_DYNNAME}'.HOSTNAME;hostname
'
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	startup
	tcpflood -m1 -M "\"<128>\""
	shutdown_when_empty
	wait_shutdown
	export RS_HOSTNAME="$(cat ${RSYSLOG_DYNNAME}.HOSTNAME)"
	rm -f "${RSYSLOG_DYNNAME}.HOSTNAME"
	echo HOSTNAME is: $RS_HOSTNAME
}


# begin a new testconfig
#	2018-09-07:	Incremented inputs.timeout.shutdown to 60000 because kafka tests may not be 
#			finished under stress otherwise
# $1 is the instance id, if given
generate_conf() {
	if [ "$RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT" == "" ]; then
		RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT="60000"
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
global(inputs.timeout.shutdown="'$RSTB_GLOBAL_INPUT_SHUTDOWN_TIMEOUT'")
$IMDiagListenPortFileName '$RSYSLOG_DYNNAME.imdiag$1.port'
$IMDiagServerRun 0

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
# $1 is file to compare
cmp_exact() {
	if [ "$1" == "" ]; then
		printf 'Testbench ERROR, cmp_exact() needs filename as %s\n' "$1"
		error_exit 100
	fi
	if [ "$EXPECTED" == "" ]; then
		printf 'Testbench ERROR, cmp_exact() needs to have env var EXPECTED set!\n'
		error_exit 100
	fi
	printf '%s\n' "$EXPECTED" | cmp - "$1"
	if [ $? -ne 0 ]; then
		printf 'invalid response generated\n'
		printf '################# %s is:\n' "$1"
		cat -n $1
		printf '################# EXPECTED was:\n'
		cat -n <<< "$EXPECTED"
		printf '\n#################### diff is:\n'
		diff - "$1" <<< "$EXPECTED"
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
	# imdiag does not genenrate the port file quickly enough on
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
	i=0
	start_timeout="$(date)"
	while test ! -f $1; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		(( i++ ))
		if test $i -gt $TB_TIMEOUT_STARTSTOP
		then
		   printf 'ABORT! Timeout waiting on startup (pid file %s1)' "$1"
		   echo "Wait initiated $start_timeout, now $(date)"
		   ls -l $1
		   ps -fp $(cat $1)
		   error_exit 1
		fi
	done
	printf '%s found, pid %s\n' "$1" "$(cat $1)"
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
			if test $i -gt $TB_TIMEOUT_STARTSTOP
			then
			   echo "ABORT! Timeout waiting on file '$2'"
			   error_exit 1
			fi
		done
		echo "$2 seen, associated pid " $(cat $1.pid)
	fi
}


# wait for the pid in $1 to terminate, abort on timeout
wait_pid_termination() {
		out_pid="$1"
		if [[ "$out_pid" == "" ]]; then
			printf 'TESTBENCH error: pidfile name not specified in wait_pid_termination\n'
			error_exit 100
		fi
		i=0
		terminated=0
		start_timeout="$(date)"
		while [[ $terminated -eq 0 ]]; do
			ps -p $out_pid &> /dev/null
			if [[ $? != 0 ]]; then
				terminated=1
			fi
			$TESTTOOL_DIR/msleep 100
			(( i++ ))
			if test $i -gt $TB_TIMEOUT_STARTSTOP ; then
			   echo "ABORT! Timeout waiting on shutdown"
			   echo "Wait initiated $start_timeout, now $(date)"
			   ps -fp $out_pid
			   echo "Instance is possibly still running and may need"
			   echo "manual cleanup."
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
echo We are waiting for kafka/zookeper being ready to deliver messages
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
	for ((i=1 ; i<=TESTMESSAGES ; i++)); do
		printf ' msgnum:%8.8d\n' $i; \
	done | kafkacat -P -b localhost:29092 -t $RANDTOPIC 2>&1 | tee >$RSYSLOG_DYNNAME.kafkacat.log
	if grep -q "All broker connections are down" $RSYSLOG_DYNNAME.kafkacat.log ; then
		printf 'SKIP: kafka has malfunctioned\n'
		error_exit 177
	fi
	if [ "$wait" == "YES" ]; then
		wait_seq_check "$@"
	fi
}

# wait for rsyslogd startup ($1 is the instance)
wait_startup() {
	wait_rsyslog_startup_pid $1
	i=0
	while test ! -f ${RSYSLOG_DYNNAME}$1.started; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		ps -p $(cat $RSYSLOG_PIDBASE$1.pid) &> /dev/null
		if [ $? -ne 0 ]
		then
		   echo "ABORT! rsyslog pid no longer active during startup!"
		   error_exit 1 stacktrace
		fi
		(( i++ ))
		if test $i -gt $TB_TIMEOUT_STARTSTOP
		then
		   echo "ABORT! Timeout waiting on startup ('${RSYSLOG_DYNNAME}.started' file)"
		   error_exit 1
		fi
	done
	echo "rsyslogd$1 startup msg seen, pid " $(cat $RSYSLOG_PIDBASE$1.pid)
	wait_file_exists $RSYSLOG_DYNNAME.imdiag$1.port
	eval export IMDIAG_PORT$1=$(cat $RSYSLOG_DYNNAME.imdiag$1.port)
	eval PORT=$IMDIAG_PORT$1
	echo "imdiag$1 port: $PORT"
	if [ "$PORT" == "" ]; then
		echo "TESTBENCH ERROR: imdiag port not found!"
		ls -l $RSYSLOG_DYNNAME*
		exit 100
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
	fi
	startup_common "$1" "$2"
	if [ "$RSTB_DAEMONIZE" == "YES" ]; then
		n_option=""
	else
		n_option="-n"
	fi
	eval LD_PRELOAD=$RSYSLOG_PRELOAD $valgrind ../tools/rsyslogd -C $n_option -i$RSYSLOG_PIDBASE$instance.pid -M../runtime/.libs:../.libs -f$CONF_FILE $RS_REDIR &
	wait_startup $instance
}


# same as startup_vg, BUT we do NOT wait on the startup message!
startup_vg_waitpid_only() {
	startup_common "$1" "$2"
	if [ "x$RS_TESTBENCH_LEAK_CHECK" == "x" ]; then
	    RS_TESTBENCH_LEAK_CHECK=full
	fi
	LD_PRELOAD=$RSYSLOG_PRELOAD valgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --gen-suppressions=all --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=$RS_TESTBENCH_LEAK_CHECK ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$2.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
	wait_rsyslog_startup_pid $1
}

# start rsyslogd with default params under valgrind control. $1 is the config file name to use
# returns only after successful startup, $2 is the instance (blank or 2!)
startup_vg() {
		startup_vg_waitpid_only $1 $2
		wait_startup $2
		echo startup_vg still running
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
	valgrind --tool=helgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --suppressions=$srcdir/linux_localtime_r.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --suppressions=$srcdir/CI/gcov.supp --gen-suppressions=all ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$2.pid -M../runtime/.libs:../.libs -f$CONF_FILE &
	wait_rsyslog_startup_pid $2
}

# start rsyslogd with default params under valgrind thread debugger control.
# $1 is the config file name to use, $2 is the instance (blank or 2!)
# returns only after successful startup
startup_vgthread() {
	startup_vgthread_waitpid_only $1 $2
	wait_startup $2
}


# inject messages via our inject interface (imdiag)
injectmsg() {
	echo injecting $2 messages
	echo injectmsg $1 $2 $3 $4 | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
}

# inject messages in INSTANCE 2 via our inject interface (imdiag)
injectmsg2() {
	echo injecting $2 messages
	echo injectmsg $1 $2 $3 $4 | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
	# TODO: some return state checking? (does it really make sense here?)
}


# show the current main queue size. $1 is the instance.
get_mainqueuesize() {
	if [ "$1" == "2" ]; then
		echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
	else
		echo getmainmsgqueuesize | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
	fi
}

# grep for (partial) content. $1 is the content to check for, $2 the file to check
# option --regex is understood, in which case $1 is a regex
content_check() {
	if [ "$1" == "--regex" ]; then
		grep_opt=
		shift
	else
		grep_opt=-F
	fi
	file=${2:-$RSYSLOG_OUT_LOG}
	if ! grep -q  $grep_opt -- "$1" < "${file}"; then
	    printf '\n============================================================\n'
	    printf 'FILE "%s" content:\n' "$file"
	    cat -n ${file}
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
		count=$(grep -c -F -- "$1" <${RSYSLOG_OUT_LOG})
		if [ $count -eq $2 ]; then
			echo content_check_with_count success, \"$1\" occured $2 times
			break
		else
			if [ "x$timecounter" == "x$timeoutend" ]; then
				shutdown_when_empty
				wait_shutdown

				echo content_check_with_count failed, expected \"$1\" to occur $2 times, but found it $count times
				echo file ${RSYSLOG_OUT_LOG} content is:
				sort < ${RSYSLOG_OUT_LOG}
				error_exit 1
			else
				echo content_check_with_count have $count, wait for $2 times $1...
				$TESTTOOL_DIR/msleep 1000
			fi
		fi
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

# check that given content $1 is not present in file $2 (default: RSYSLOG_OUTLOG)
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

# wait for main message queue to be empty. $1 is the instance.
wait_queueempty() {
	if [ "$1" == "2" ]; then
		echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT2 || error_exit  $?
	else
		echo WaitMainQueueEmpty | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
	fi
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
	if [ "$USE_VALGRIND" == "YES" ]; then
		wait_shutdown_vg "$1"
		return
	fi
	i=0
	out_pid=$(cat $RSYSLOG_PIDBASE$1.pid.save)
	printf '%s wait on shutdown of %s\n' "$(tb_timestamp)" "$out_pid"
	if [[ "$out_pid" == "" ]]
	then
		terminated=1
	else
		terminated=0
	fi
	start_timeout="$(date)"
	while [[ $terminated -eq 0 ]]; do
		ps -p $out_pid &> /dev/null
		if [[ $? != 0 ]]; then
			terminated=1
		fi
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		(( i++ ))
		if test $i -gt $TB_TIMEOUT_STARTSTOP
		then
		   echo "ABORT! Timeout waiting on shutdown"
		   echo "Wait initiated $start_timeout, now $(date)"
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
	if [ -e core.* ]
	then
	   echo "ABORT! core file exists"
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
# $3 timout in seconds
# options (need to be specified in THIS ORDER if multiple given):
# --delay ms - if given, delay to use between retries
# --abort-on-oversize - error_exit if more lines than expected are present
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
	timeout=${3:-$TB_TEST_TIMEOUT}
	timeoutbegin=$(date +%s)
	timeoutend=$(( timeoutbegin + timeout ))
	# TODO: change this to support odl mode, if needed: timeoutend=${3:-200}
	file=${1:-$RSYSLOG_OUT_LOG}
	waitlines=${2:-$NUMMESSAGES}

	while true ; do
		if [ -f "$file" ]; then
			count=$(wc -l < "$file")
		fi
		if [ $abort_oversize == "YES" ] && [ ${count:=0} -gt $waitlines ]; then
			printf 'FAIL: wait_file_lines, too many lines, expected %d, current %s, took %s seconds\n'  $waitlines $count, "$(( $(date +%s) - timeoutbegin ))"
			error_exit 1
		fi
		if [ ${count:=0} -eq $waitlines ]; then
			echo wait_file_lines success, have $waitlines lines, took $(( $(date +%s) - timeoutbegin )) seconds
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
	unset count
}


# wait until seq_check succeeds. This is used to synchronize various
# testbench timing-related issues, most importantly rsyslog shutdown
# all parameters are passed to seq_check
wait_seq_check() {
	timeoutend=$(( $(date +%s) + TB_TEST_TIMEOUT ))

	while true ; do
		if [ -f "$RSYSLOG_OUT_LOG" ]; then
			count=$(wc -l < "$RSYSLOG_OUT_LOG")
		fi
		seq_check --check-only "$@" &>/dev/null
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
	kill -HUP $(cat $RSYSLOG_PIDBASE$1.pid)
	$TESTTOOL_DIR/msleep 1000
}


# actually, we wait for rsyslog.pid to be deleted. $1 is the instance
wait_shutdown_vg() {
	wait $(cat $RSYSLOG_PIDBASE$1.pid)
	export RSYSLOGD_EXIT=$?
	echo rsyslogd run exited with $RSYSLOGD_EXIT

	if [ $(ls vgcore.* 2> /dev/null | wc -l) -gt 0 ]; then
	   printf 'ABORT! core file exists:\n'
	   ls -l vgcore.*
	   error_exit 1
	fi
	if [ "$USE_VALGRIND" == "YES" ]; then
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
#       call it immeditely before termination. This may be used to cleanup
#       some things or emit additional diagnostic information.
error_exit() {
	if [ -e core* ]
	then
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
		if [ -e core* ]
		then
			echo trying to analyze core for main rsyslogd binary
			echo note: this may not be the correct file, check it
			CORE=$(ls core*)
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
	# output listening ports as a temporay debug measure (2018-09-08 rgerhards), now disables, but not yet removed (2018-10-22)
	#if [ $(uname) == "Linux" ]; then
	#	netstat -tlp
	#else
	#	netstat
	#fi

	# Extended debug output for dependencies started by testbench
	if [[ "$EXTRA_EXITCHECK" == 'dumpkafkalogs' ]]; then
		# Dump Zookeeper log
		dump_zookeeper_serverlog
		# Dump Kafka log
		dump_kafka_serverlog
	fi

	# Extended Exit handling for kafka / zookeeper instances 
	kafka_exit_handling "false"

	error_stats $1 # Report error to rsyslog testbench stats
	do_cleanup

	if [ "$TEST_STATUS" == "unreliable" ] && [ "$1" -ne 100 ]; then
		# TODO: log github issue
		printf 'Test flagged as unreliable, exiting with SKIP. Original exit code was %d\n' "$1"
		printf 'GitHub ISSUE: %s\n' "$TEST_GITHUB_ISSUE"
		exit 77
	else
		if [ "$1" -eq 177 ]; then
			exit 77
		fi
		exit $1
	fi
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
		testname=$($srcdir/urlencode.py "$RSYSLOG_TESTNAME")
		testenv=$($srcdir/urlencode.py "${VCS_SLUG:-$PWD}")
		testmachine=$($srcdir/urlencode.py "$HOSTNAME")
		logurl=$($srcdir/urlencode.py "${CI_BUILD_URL:-}")
		wget -nv $RSYSLOG_STATSURL\?Testname=$testname\&Testenv=$testenv\&Testmachine=$testmachine\&exitcode=${1:-1}\&logurl=$logurl\&rndstr=jnxv8i34u78fg23
	fi
}

# do the usual sequence check to see if everything was properly received.
# $4... are just to have the abilit to pass in more options...
# add -v to chkseq if you need more verbose output
# argument --check-only can be used to simply do a check without abort in fail case
seq_check() {
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
	if [ ! -f "$RSYSLOG_OUT_LOG" ]; then
		if [ "$check_only"  == "YES" ]; then
			return 1
		fi
		printf 'FAIL: %s does not exist in seq_check!\n' "$RSYSLOG_OUT_LOG"
		error_exit 1
	fi
	$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${RSYSLOG_OUT_LOG} | ./chkseq -s$startnum -e$endnum $3 $4 $5 $6 $7
	ret=$?
	if [ "$check_only"  == "YES" ]; then
		return $ret
	fi
	if [ $ret -ne 0 ]; then
		$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${RSYSLOG_OUT_LOG} > $RSYSLOG_DYNNAME.error.log
		echo "sequence error detected in $RSYSLOG_OUT_LOG"
		echo "number of lines in file: $(wc -l $RSYSLOG_OUT_LOG)"
		echo "sorted data has been placed in error.log, first 10 lines are:"
		cat -n $RSYSLOG_DYNNAME.error.log | head -10
		echo "---last 10 lines are:"
		cat -n $RSYSLOG_DYNNAME.error.log | tail -10
		echo "UNSORTED data, first 10 lines are:"
		cat -n $RSYSLOG_OUT_LOG | head -10
		echo "---last 10 lines are:"
		cat -n $RSYSLOG_OUT_LOG | tail -10
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
# $4... are just to have the abilit to pass in more options...
# add -v to chkseq if you need more verbose output
seq_check2() {
	$RS_SORTCMD $RS_SORT_NUMERIC_OPT < ${RSYSLOG2_OUT_LOG}  | ./chkseq -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do the usual sequence check, but for gzip files
# $4... are just to have the abilit to pass in more options...
gzip_seq_check() {
	ls -l ${RSYSLOG_OUT_LOG}
	gunzip < ${RSYSLOG_OUT_LOG} | $RS_SORTCMD $RS_SORT_NUMERIC_OPT | ./chkseq -v -s$1 -e$2 $3 $4 $5 $6 $7
	if [ "$?" -ne "0" ]; then
		echo "sequence error detected"
		error_exit 1
	fi
}


# do a tcpflood run and check if it worked params are passed to tcpflood
tcpflood() {
	eval ./tcpflood -p$TCPFLOOD_PORT "$@" $TCPFLOOD_EXTRA_OPTS
	if [ "$?" -ne "0" ]; then
		echo "error during tcpflood on port ${TCPFLOOD_PORT}! see ${RSYSLOG_OUT_LOG}.save for what was written"
		cp ${RSYSLOG_OUT_LOG} ${RSYSLOG_OUT_LOG}.save
		error_exit 1 stacktrace
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
	rm -f rsyslog.conf.tlscert stat-file1 rsyslog.empty imfile-state*
	rm -rf rsyslog-link.*.log targets
	rm -f ${TESTCONF_NM}.conf
	rm -f tmp.qi nocert
	rm -fr $RSYSLOG_DYNNAME*  # delete all of our dynamic files
	unset TCPFLOOD_EXTRA_OPTS

	# Extended Exit handling for kafka / zookeeper instances 
	kafka_exit_handling "true"

	printf 'Test SUCCESFUL\n'
	echo  -------------------------------------------------------------------------------
	exit 0
}

# finds a free port that we can bind a listener to
# Obviously, any solution is race as another process could start
# just after us and grab the same port. However, in practice it seems
# to work pretty well. In any case, we should probably call this as
# late as possible before the usage of the port.
get_free_port() {
python -c 'import socket; s=socket.socket(); s.bind(("", 0)); print(s.getsockname()[1]); s.close()'
}


# return the inode number of file $1; file MUST exist
get_inode() {
	if [ ! -f "$1" ]; then
		printf 'FAIL: file "%s" does not exist in get_inode\n' "$1"
		error_exit 100
	fi
	stat -c '%i' "$1"
}


# check if command $1 is available - will exit 77 when not OK
check_command_available() {
	command -v $1
	if [ $? -ne 0 ] ; then
		echo Testbench requires unavailable command: $1
		exit 77
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
dep_cache_dir=$(pwd)/.dep_cache
dep_zk_url=http://www-us.apache.org/dist/zookeeper/zookeeper-3.4.13/zookeeper-3.4.13.tar.gz
dep_zk_cached_file=$dep_cache_dir/zookeeper-3.4.13.tar.gz

# byANDRE: We stay with kafka 0.10.x for now. Newer Kafka Versions have changes that
#	makes creating testbench with single kafka instances difficult.
# old version -> dep_kafka_url=http://www-us.apache.org/dist/kafka/0.10.2.2/kafka_2.12-0.10.2.2.tgz
# old version -> dep_kafka_cached_file=$dep_cache_dir/kafka_2.12-0.10.2.2.tgz
dep_kafka_url=http://www-us.apache.org/dist/kafka/2.0.0/kafka_2.11-2.0.0.tgz
dep_kafka_cached_file=$dep_cache_dir/kafka_2.11-2.0.0.tgz

if [ -z "$ES_DOWNLOAD" ]; then
	export ES_DOWNLOAD=elasticsearch-5.6.9.tar.gz
fi
dep_es_cached_file="$dep_cache_dir/$ES_DOWNLOAD"

# kafaka (including Zookeeper)
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
			# Prozess shutdown, do cleanup now
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
			# Prozess shutdown, do cleanup now
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
			printf 'zookeeper already runing, no need to start\n'
			return
		else
			printf 'INFO: zookeper pidfile %s exists, but zookeeper not runing\n' "$ZOOPIDFILE"
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
	# Force IPv4 usage of Kafka!
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
		printf 'kafka already runing, no need to start\n'
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
			stop_zookeeper; stop_kafka
			start_zookeeper; start_kafka
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
			dep_es_url="https://artifacts.elastic.co/downloads/elasticsearch/$ES_DOWNLOAD"
			printf 'ElasticSearch: satisfying dependency %s from %s\n' "$ES_DOWNLOAD" "$dep_es_url"
			wget -q $dep_es_url -O $dep_es_cached_file
		fi
	fi
}


# prepare eleasticsearch execution environment
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
	else
		cp -f $srcdir/testsuites/$dep_work_es_config $dep_work_dir/es/config/elasticsearch.yml
	fi

	if [ ! -d $dep_work_dir/es/data ]; then
			echo "Creating elastic search directories"
			mkdir -p $dep_work_dir/es/data
			mkdir -p $dep_work_dir/es/logs
			mkdir -p $dep_work_dir/es/tmp
	fi
	echo ElasticSearch prepared for use in test.
}


# $2, if set, is the number of additional ES instances
start_elasticsearch() {
	# Heap Size (limit to 128MB for testbench! defaults is way to HIGH)
	export ES_JAVA_OPTS="-Xms128m -Xmx128m"

	dep_work_dir=$(readlink -f .dep_wrk)
	dep_work_es_config="es.yml"
	dep_work_es_pidfile="$(pwd)/es.pid"
	echo "Starting ElasticSearch"

	# THIS IS THE ACTUAL START of ES
	$dep_work_dir/es/bin/elasticsearch -p $dep_work_es_pidfile -d
	$TESTTOOL_DIR/msleep 2000
	# TODO: wait pidfile!
	printf 'elasticsearch pid is %s\n' "$(cat $dep_work_es_pidfile)"

	# Wait for startup with hardcoded timeout
	timeoutend=60
	timeseconds=0
	# Loop until elasticsearch port is reachable or until
	# timeout is reached!
	until [ "$(curl --silent --show-error --connect-timeout 1 http://localhost:${ES_PORT:-19200} | grep 'rsyslog-testbench')" != "" ]; do
		echo "--- waiting for ES startup: $timeseconds seconds"
		$TESTTOOL_DIR/msleep 1000
		(( timeseconds=timeseconds + 1 ))

		if [ "$timeseconds" -gt "$timeoutend" ]; then 
			echo "--- TIMEOUT ( $timeseconds ) reached!!!"
			error_exit 1
		fi
	done
	$TESTTOOL_DIR/msleep 2000
	echo ES startup succeeded
}

# read data from ES to a local file so that we can process
# $1 - number of records (ES does not return all records unless you tell it explicitely).
# $2 - ES port
es_getdata() {
	curl --silent localhost:${2:-9200}/rsyslog_testbench/_search?size=$1 > $RSYSLOG_DYNNAME.work
	python $srcdir/es_response_get_msgnum.py > ${RSYSLOG_OUT_LOG}
}


stop_elasticsearch() {
	dep_work_dir=$(readlink -f $srcdir)
	dep_work_es_pidfile="es.pid"
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
		#for pid in $(ps -eo pid,args|grep '/tools/[r]syslogd ' |sed -e 's/\( *\)\([0-9]*\).*/\2/');
		#do
			#echo "ERROR: left-over previous instance $pid, killing it"
			#ps -fp $pid
			#pwd
			#kill -9 $pid
		#done
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
		if [ "$RSYSLOG_DYNNAME" != "" ]; then
			echo "FAIL: \$RSYSLOG_DYNNAME already set in init"
			echo "hint: was init accidently called twice?"
			exit 2
		fi
		export RSYSLOG_DYNNAME="rstb_$(./test_id $(basename $0))"
		export RSYSLOG2_OUT_LOG=rsyslog2.out.log # TODO: remove
		export RSYSLOG_OUT_LOG=rsyslog.out.log # TODO: remove
		export RSYSLOG_PIDBASE="rsyslog" # TODO: remove
		export IMDIAG_PORT=13500
		export IMDIAG_PORT2=13501
		export TCPFLOOD_PORT=13514

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
		printf '------------------------------------------------------------\n'
		printf '%s Test: %s\n' "$(tb_timestamp)" "$0"
		printf '------------------------------------------------------------\n'
		rm -f xlate*.lkp_tbl
		rm -f log log* # RSyslog debug output 
		rm -f work 
		rm -rf test-logdir stat-file1
		rm -f rsyslog.empty imfile-state* omkafka-failed.data
		rm -rf rsyslog-link.*.log targets
		rm -f tmp.qi nocert
		rm -f core.* vgcore.* core*
		# Note: rsyslog.action.*.include must NOT be deleted, as it
		# is used to setup some parameters BEFORE calling init. This
		# happens in chained test scripts. Delete on exit is fine,
		# though.
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
			export valgrind="valgrind --malloc-fill=ff --free-fill=fe --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --log-fd=1"
		fi
		;;

   'check-ipv6-available')   # check if IPv6  - will exit 77 when not OK
		ifconfig -a |grep ::1
		if [ $? -ne 0 ] ; then
			printf 'this test requires an active IPv6 stack, which we do not have here\n'
			exit 77
		fi
		;;
   'getpid')
		pid=$(cat $RSYSLOG_PIDBASE$2.pid)
		;;
   'kill-immediate') # kill rsyslog unconditionally
		kill -9 $(cat $RSYSLOG_PIDBASE.pid)
		# note: we do not wait for the actual termination!
		;;
    'injectmsg-litteral') # inject litteral-payload  via our inject interface (imdiag)
		echo injecting msg payload from: $2
		sed -e 's/^/injectmsg litteral /g' < "$2" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
		# TODO: some return state checking? (does it really make sense here?)
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
		prev_count=$(grep 'BEGIN$' <$2 | wc -l)
		new_count=$prev_count
		while [[ "x$prev_count" == "x$new_count" ]]; do
				# busy spin, because it allows as close timing-coordination in actual test run as possible
				new_count=$(grep -c'BEGIN$' <"$2")
		done
		echo "stats push registered"
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
   'first-column-sum-check') 
		sum=$(grep $3 < $4 | sed -e $2 | awk '{s+=$1} END {print s}')
		if [ "x${sum}" != "x$5" ]; then
		    printf '\n============================================================\n'
		    echo FAIL: sum of first column with edit-expr "'$2'" run over lines from file "'$4'" matched by "'$3'" equals "'$sum'" which is NOT equal to EXPECTED value of "'$5'"
		    echo "file contents:"
		    cat $4
		    error_exit 1
		fi
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
