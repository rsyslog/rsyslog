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
# RSYSLOG_TESTBENCH_EXTERNAL_ES_URL
#		Base URL (protocol, host, and port) of an externally managed
#		Elasticsearch instance that the testbench should use instead of
#		starting its own container. When set, the test harness skips all
#		local Elasticsearch start/stop handling and assumes that the
#		specified endpoint is reachable and dedicated to the tests.
# RSYSLOG_TESTBENCH_ES_MAJOR_VERSION
#               Detected Elasticsearch major version exposed to the test suite.
#               Populated automatically by ensure_elasticsearch_ready().
#
#
# EXIT STATES
# 0 - ok
# 1 - FAIL
# 77 - SKIP
# 100 - Testbench failure
export TB_ERR_TIMEOUT=101
# Default module directory path for rsyslogd startup/config checks.
RSYSLOG_MODDIR=${RSYSLOG_MODDIR:-"../runtime/.libs:../.libs"}
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
# Check if valgrind is enabled, use longer timeout if so
if [ "$USE_VALGRIND" == "YES" ] || [ "$USE_VALGRIND" == "YES-NOLEAK" ]; then
	TB_STARTUP_MAX_RUNTIME=${STARTUP_MAX_RUNTIME:-240} # maximum runtime in seconds for rsyslog startup with valgrind;
else
	TB_STARTUP_MAX_RUNTIME=${STARTUP_MAX_RUNTIME:-60} # maximum runtime in seconds for rsyslog startup;
fi
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
    if ! curl -L --fail --max-time 30 "${http_endpoint}" 1>/dev/null 2>&1; then
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

# function to skip a test if TSAN is enabled
# This is necessary as TSAN does not properly handle thread creation
# after fork() - which happens regularly in rsyslog if backgrounding
# is activated.
# $1 is the reason why TSAN is not supported
# note: we depend on CFLAGS to properly reflect build options (what
#       usually is the case when the testbench is run)
skip_TSAN() {
        echo skip:_TSAN, CFLAGS $CFLAGS
        if [[ "$CFLAGS" == *"sanitize=thread"* ]]; then
                printf 'test incompatible with TSAN because of %s\n' "$1"
                exit 77
        fi
}


# ensure a dynamically loaded rsyslog plugin is available before continuing.
# $1 - plugin name (without the leading im/om/mm prefix differentiation)
# $2 - optional module directory override relative to the current working dir
require_plugin() {
        if [ -z "$1" ]; then
                printf 'TESTBENCH_ERROR: require_plugin requires a plugin name\n'
                error_exit 100
        fi

        local plugin="$1"
        local module_root
        if [ -n "$2" ]; then
                module_root="$2"
        else
                module_root="../plugins/${plugin}"
        fi

        local candidates=(
                "${module_root}/.libs/${plugin}.so"
                "${module_root}/.libs/${plugin}.la"
                "${module_root}/${plugin}.so"
                "${module_root}.so"
        )

        for candidate in "${candidates[@]}"; do
                if [ -f "$candidate" ]; then
                        return 0
                fi
        done

        printf 'info: skipping test - plugin %s not available (checked %s)\n' \
                "$plugin" "${candidates[*]}"
        exit 77
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
		RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE="30000"
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
       default.action.queue.timeoutEnqueue="'$RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE'"
       debug.abortOnProgramError="on")
# use legacy-style for the following settings so that we can override if needed
$MainmsgQueueTimeoutEnqueue '$RSTB_ACTION_DEFAULT_Q_TO_ENQUEUE'
$MainmsgQueueTimeoutShutdown '$RSTB_GLOBAL_QUEUE_SHUTDOWN_TIMEOUT'
$IMDiagListenPortFileName '$RSYSLOG_DYNNAME.imdiag$1.port'
$IMDiagServerRun 0
$IMDiagAbortTimeout '$TB_TEST_MAX_RUNTIME'

:syslogtag, contains, "rsyslogd"  ./'${RSYSLOG_DYNNAME}$1'.started
###### end of testbench instrumentation part, test conf follows:' > ${TESTCONF_NM}$1.conf
	# Optionally enforce IPv4 for this test instance.
	# Set RSTB_FORCE_IPV4=1 to force, or set RSTB_NET_IPPROTO explicitly
	# to one of: unspecified | ipv4-only | ipv6-only.
	if [ "$RSTB_FORCE_IPV4" = "1" ] && [ -z "$RSTB_NET_IPPROTO" ]; then
		RSTB_NET_IPPROTO="ipv4-only"
	fi
	if [ -n "$RSTB_NET_IPPROTO" ]; then
		add_conf "global(net.ipprotocol=\"${RSTB_NET_IPPROTO}\")" "$1"
	fi
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

# run rsyslogd config check (-N1) with default module dirs.
# $1: config file name (optional), $2: instance (optional)
rsyslogd_config_check() {
	startup_common "$1" "$2"
	eval LD_PRELOAD=$RSYSLOG_PRELOAD ../tools/rsyslogd -C -N1 -M"$RSYSLOG_MODDIR" -f$CONF_FILE $RS_REDIR
}

# wait for appearance of a specific pid file, given as $1
wait_startup_pid() {
	if [ "$1" == "" ]; then
		echo "FAIL: testbench bug: wait_startup_called without \$1"
		error_exit 100
	fi
	while test ! -f $1; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_STARTUP_MAX_RUNTIME )) ]; then
		   printf '%s ABORT! Timeout waiting on startup (pid file %s) after %d seconds\n' "$(tb_timestamp)" "$1" $TB_STARTUP_MAX_RUNTIME
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

# Wait until a TCP port accepts connections. $1=host, $2=port,
# $3(optional)=timeout in seconds, $4(optional)=description for log output.
wait_for_tcp_service() {
        local host="$1"
        local port="$2"
        local timeout="${3:-60}"
        local description="${4:-${host}:${port}}"
        local start_ts
        local elapsed
        local iteration=0
	local python_bin="${PYTHON:-}"
	local have_nc=0

        if [ -z "$host" ] || [ -z "$port" ]; then
                printf 'wait_for_tcp_service: host (%s) or port (%s) missing\n' "$host" "$port"
                return 1
        fi

	if [ -n "$python_bin" ] && ! command -v "$python_bin" >/dev/null 2>&1; then
		python_bin=""
	fi
	if [ -z "$python_bin" ]; then
		if command -v python3 >/dev/null 2>&1; then
			python_bin=$(command -v python3)
		elif command -v python >/dev/null 2>&1; then
			python_bin=$(command -v python)
		fi
	fi

	if command -v nc >/dev/null 2>&1; then
		have_nc=1
	fi

	if [ $have_nc -eq 0 ] && [ -z "$python_bin" ]; then
		printf 'wait_for_tcp_service: neither "nc" nor a Python interpreter is available for %s:%s\n' "$host" "$port"
		return 1
	fi

        start_ts=$(date +%s)
        while true; do
		local connected=1
		if [ $have_nc -eq 1 ]; then
			if nc -w1 -z "$host" "$port" >/dev/null 2>&1; then
				connected=0
			fi
		fi

		if [ $connected -ne 0 ] && [ -n "$python_bin" ]; then
			if "$python_bin" - "$host" "$port" <<'PY' >/dev/null 2>&1
import socket
import sys

host = sys.argv[1]
port = int(sys.argv[2])

try:
	with socket.create_connection((host, port), timeout=1):
		pass
except OSError:
	sys.exit(1)

sys.exit(0)
PY
			then
				connected=0
			fi
		fi

		if [ $connected -eq 0 ]; then
                        printf '%s %s reachable on %s:%s\n' "$(tb_timestamp)" "$description" "$host" "$port"
                        return 0
                fi

                elapsed=$(( $(date +%s) - start_ts ))
                if [ "$elapsed" -ge "$timeout" ]; then
                        printf '%s ERROR: timeout waiting for %s (%s:%s)\n' "$(tb_timestamp)" "$description" "$host" "$port"
                        return 1
                fi

                if (( iteration % 5 == 0 )); then
                        printf '%s waiting for %s (%s:%s) - elapsed %ss\n' "$(tb_timestamp)" "$description" "$host" "$port" "$elapsed"
                fi

                $TESTTOOL_DIR/msleep 200
                (( iteration++ ))
        done
}

# Helper to obtain listener endpoints (host:port) from a Kafka server config.
_kafka_listeners_from_config() {
        local config_path="$1"
        local listeners_line
        local value
        local entry
        local host
        local port

        if [ ! -f "$config_path" ]; then
                return
        fi

        listeners_line=$(grep -i '^listeners=' "$config_path" | tail -n1)
        if [ -z "$listeners_line" ]; then
                printf '127.0.0.1:9092\n'
                return
        fi

        value=${listeners_line#*=}
        IFS=',' read -ra __kafka_listener_entries <<< "$value"
        for entry in "${__kafka_listener_entries[@]}"; do
                entry=$(echo "$entry" | tr -d '[:space:]')
                if [ -z "$entry" ]; then
                        continue
                fi
                entry=${entry#*://}
                host=${entry%:*}
                port=${entry##*:}
                if [ -z "$port" ]; then
                        continue
                fi
                if [ -z "$host" ] || [ "$host" = "0.0.0.0" ]; then
                        host="127.0.0.1"
                fi
                printf '%s:%s\n' "$host" "$port"
        done
}

# Populate the Kafka dependency workspace directory and config file name for an
# optional instance suffix provided via $1. The computed values are written to
# the variables whose names are supplied via $2 (directory) and $3 (config).
_kafka_instance_layout() {
        local instance="$1"
        local dir_var="$2"
        local config_var="$3"
        local dir
        local config

        if [ -z "$instance" ]; then
                dir=$(readlink -f .dep_wrk)
                config="kafka-server.properties"
        else
                dir=$(readlink -f "$instance")
                config="kafka-server${instance}.properties"
        fi

        printf -v "$dir_var" '%s' "$dir"
        printf -v "$config_var" '%s' "$config"
}

# Wait for Kafka brokers configured via $2 (config path) under $1 (kafka dir).
# Accepts optional timeout (seconds) as third parameter.
wait_for_kafka_ready_internal() {
        local kafka_dir="$1"
        local config_path="$2"
        local timeout="${3:-60}"
        local -a endpoints=()
        local endpoint
        local last_topics_output=""
        local start_ts
        local elapsed
        local iteration=0
        local bootstrap_csv
        local output

        if [ ! -d "$kafka_dir" ]; then
                printf 'wait_for_kafka_ready_internal: missing kafka dir %s\n' "$kafka_dir"
                return 1
        fi

        while IFS= read -r endpoint; do
                [ -n "$endpoint" ] && endpoints+=("$endpoint")
        done < <(_kafka_listeners_from_config "$config_path")

        if [ ${#endpoints[@]} -eq 0 ]; then
                endpoints=("127.0.0.1:9092")
        fi

        local IFS=,
        bootstrap_csv="${endpoints[*]}"
        unset IFS

        start_ts=$(date +%s)
        while true; do
                if output=$(cd "$kafka_dir" && ./bin/kafka-topics.sh --bootstrap-server "$bootstrap_csv" --list 2>&1); then
                        elapsed=$(( $(date +%s) - start_ts ))
                        printf '%s kafka-topics --list succeeded for %s after %ss\n' "$(tb_timestamp)" "$bootstrap_csv" "$elapsed"
                        return 0
                else
                        last_topics_output=$output
                fi

                elapsed=$(( $(date +%s) - start_ts ))
                if [ "$elapsed" -ge "$timeout" ]; then
                        printf '%s ERROR: kafka brokers %s not ready after %ss\n' "$(tb_timestamp)" "$bootstrap_csv" "$timeout"
                        if [ -n "$last_topics_output" ]; then
                                printf '%s last kafka-topics output:\n%s\n' "$(tb_timestamp)" "$last_topics_output"
                        fi
                        return 1
                fi

                if (( iteration % 5 == 0 )); then
                        printf '%s waiting for kafka-topics --list to succeed for %s (elapsed %ss)\n' "$(tb_timestamp)" "$bootstrap_csv" "$elapsed"
                fi

                $TESTTOOL_DIR/msleep 200
                (( iteration++ ))
        done
}

wait_for_kafka_startup() {
        local instance="$1"
        local timeout="${2:-60}"
        local dep_work_dir
        local dep_work_kafka_config

        _kafka_instance_layout "$instance" dep_work_dir dep_work_kafka_config

        wait_for_kafka_ready_internal "$dep_work_dir/kafka" "$dep_work_dir/kafka/config/$dep_work_kafka_config" "$timeout"
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

# inject messages via kcat tool (for imkafka tests)
# $1 == "--wait" means wait for rsyslog to receive TESTMESSAGES lines in RSYSLOG_OUT_LOG
# $TESTMESSAGES contains number of messages to inject
# $RANDTOPIC contains topic to produce to
injectmsg_kcat() {
	if [ "$1" == "--wait" ]; then
		wait="YES"
		shift
	fi
	if [ "$TESTMESSAGES" == "" ]; then
		printf 'TESTBENCH ERROR: TESTMESSAGES env var not set!\n'
		error_exit 1
	fi
	MAXATONCE=25000 # how many msgs should kcat send? - hint: current version errs out above ~70000
	i=1
	while (( i<=TESTMESSAGES )); do
		currmsgs=0
		while ((i <= $TESTMESSAGES && currmsgs != MAXATONCE)); do
			printf ' msgnum:%8.8d\n' $i;
			i=$((i + 1))
			currmsgs=$((currmsgs+1))
		done  > "$RSYSLOG_DYNNAME.kcat.in"
		set -e
		kcat -P -b 127.0.0.1:29092 -t $RANDTOPIC <"$RSYSLOG_DYNNAME.kcat.in" 2>&1 | tee >$RSYSLOG_DYNNAME.kcat.log
		set +e
		printf 'kcat injected %d msgs so far\n' $((i - 1))
		kafka_check_broken_broker $RSYSLOG_DYNNAME.kcat.log
		check_not_present "ERROR" $RSYSLOG_DYNNAME.kcat.log
		cat $RSYSLOG_DYNNAME.kcat.log
	done

	if [ "$wait" == "YES" ]; then
		wait_seq_check "$@"
	fi
}


# check if rsyslog process is active
# $1 - instance ID
# exits with error if process is not active
check_rsyslog_active() {
	local instance_id=$1
	local pid_file="$RSYSLOG_PIDBASE$instance_id.pid"

	if [ ! -f "$pid_file" ]; then
		echo "ABORT! rsyslog pid file $pid_file does not exist!"
		error_exit 1 stacktrace
	fi

	if ! ps -p "$(cat "$pid_file")" &> /dev/null; then
		echo "ABORT! rsyslog pid no longer active!"
		error_exit 1 stacktrace
	fi
}

# wait for rsyslogd startup ($1 is the instance)
wait_startup() {
	local instance=$1
	local pid_file="$RSYSLOG_PIDBASE$instance.pid"
	local started_file="${RSYSLOG_DYNNAME}$instance.started"
	local imdiag_port_file="$RSYSLOG_DYNNAME.imdiag$instance.port"
	# Wait until any early startup indicator appears: pid file, started marker, or imdiag port file
	while :; do
		if [ -f "$pid_file" ] || [ -f "$started_file" ] || { [ -f "$imdiag_port_file" ] && [ "$(cat "$imdiag_port_file" 2>/dev/null)" != "" ]; }; then
			break
		fi
		# If we are exactly at/over timeout threshold, re-check once more before aborting
		if [ $(date +%s) -gt $(( TB_STARTTEST + TB_STARTUP_MAX_RUNTIME )) ]; then
			if [ -f "$pid_file" ] || [ -f "$started_file" ] || { [ -f "$imdiag_port_file" ] && [ "$(cat "$imdiag_port_file" 2>/dev/null)" != "" ]; }; then
				break
			fi
			printf '%s ABORT! Timeout waiting startup indicator (%s or %s or %s) after %d seconds\n' "$(tb_timestamp)" "$pid_file" "$started_file" "$imdiag_port_file" $TB_STARTUP_MAX_RUNTIME
			# show current file states for diagnostics
			ls -l "$pid_file" "$started_file" "$imdiag_port_file" 2>/dev/null || true
			# observed late creation on some CI runners; proceed softly and let subsequent waits validate
			break
		fi
		$TESTTOOL_DIR/msleep 50 # wait 50 milliseconds
	done
	# If we have a pid file, perform a quick liveness check
	if [ -f "$pid_file" ]; then
		$TESTTOOL_DIR/msleep 50
		check_rsyslog_active $instance
	fi
	if [ -f "$pid_file" ]; then
		echo "$(tb_timestamp) rsyslogd$instance startup indicator seen, pid " $(cat "$pid_file")
	else
		echo "$(tb_timestamp) rsyslogd$instance startup indicator seen (pid pending)"
	fi
	# Ensure we have the imdiag port
	wait_file_exists "$imdiag_port_file"
	export "IMDIAG_PORT$instance"=$(cat "$imdiag_port_file")
	local port_var="IMDIAG_PORT$instance"
	local PORT="${!port_var}"
	echo "imdiag$instance port: $PORT"
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
	eval LD_PRELOAD=$RSYSLOG_PRELOAD $valgrind ../tools/rsyslogd -C $n_option -i$RSYSLOG_PIDBASE$instance.pid -M"$RSYSLOG_MODDIR" -f$CONF_FILE $RS_REDIR &
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

# start a minitcpsrvr with the regular parameters
# $1 is the output file name
# $2 is the instance name (1, 2)
start_minitcpsrvr() {
	instance=${2:-1}
	./minitcpsrv -t127.0.0.1 -p 0 -P "$RSYSLOG_DYNNAME.minitcpsrvr_port$instance" -f "$1" $MINITCPSRV_EXTRA_OPTS &
	BGPROCESS=$!
	wait_file_exists "$RSYSLOG_DYNNAME.minitcpsrvr_port$instance"
	if [ "$instance" == "1" ]; then
		export MINITCPSRVR_PORT1="$(cat $RSYSLOG_DYNNAME.minitcpsrvr_port$instance)"
	else
		export MINITCPSRVR_PORT2="$(cat $RSYSLOG_DYNNAME.minitcpsrvr_port$instance)"
	fi
	echo "### background minitcpsrv process id is $BGPROCESS port $(cat $RSYSLOG_DYNNAME.minitcpsrvr_port$instance) ###"
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
	LD_PRELOAD=$RSYSLOG_PRELOAD valgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --gen-suppressions=all --log-fd=1 --error-exitcode=10 --malloc-fill=ff --free-fill=fe --leak-check=$RS_TESTBENCH_LEAK_CHECK ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$instance.pid -M"$RSYSLOG_MODDIR" -f$CONF_FILE &
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
	valgrind --tool=helgrind $RS_TEST_VALGRIND_EXTRA_OPTS $RS_TESTBENCH_VALGRIND_EXTRA_OPTS --log-fd=1 --error-exitcode=10 --suppressions=$srcdir/linux_localtime_r.supp --suppressions=$srcdir/known_issues.supp ${EXTRA_VALGRIND_SUPPRESSIONS:-} --suppressions=$srcdir/CI/gcov.supp --gen-suppressions=all ../tools/rsyslogd -C -n -i$RSYSLOG_PIDBASE$2.pid -M"$RSYSLOG_MODDIR" -f$CONF_FILE &
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


## Inject messages via our inject interface (imdiag).
## $1 is start message number, env var NUMMESSAGES is number of messages to inject.
injectmsg() {
	if [ "$3" != "" ] ; then
		printf 'error: injectmsg only has two arguments, extra arg is %s\n' "$3"
	fi
	msgs=${2:-${NUMMESSAGES:-1}}
	echo injecting $msgs messages
	echo injectmsg "${1:-0}" "$msgs" | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
}

## Inject messages in INSTANCE 2 via our inject interface (imdiag).
injectmsg2() {
	msgs=${2:-${NUMMESSAGES:-1}}
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
	count=$(grep -c $grep_opt -- "$1" <${file})
	if [ ${count:=0} -ne "$2" ]; then
	    grep -c -F -- "$1" <${file}
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
	printf '%s Shutting down instance %s\n' "$(date +%H:%M:%S)" ${1:-1}
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
	# Check for test-specific core files first (prevents Issue #6268 race condition)
	core_found=0
	if [ -n "$RSYSLOG_DYNNAME" ]; then
		if [ "$(ls ${RSYSLOG_DYNNAME}.core core.${RSYSLOG_DYNNAME}.* 2>/dev/null)" != "" ]; then
			core_found=1
		fi
	fi
	# Also check generic cores (for backward compatibility)
	if [ $core_found -eq 0 ] && [ "$(ls core.* 2>/dev/null)" != "" ]; then
		core_found=1
	fi
	if [ $core_found -eq 1 ]; then
	   printf 'ABORT! core file exists (maybe from a parallel run!)\n'
	   pwd
	   ls -l core.* ${RSYSLOG_DYNNAME}.core core.${RSYSLOG_DYNNAME}.* 2>/dev/null
	   error_exit  1
	fi
}


# wait for HUP to complete. $1 is the instance
# note: there is a slight chance HUP was not completed. This can happen if it takes
# the system very long (> 500ms) to receive the HUP and set the internal flag
# variable. aka "very very low probability".
await_HUP_processed() {
	if [ "$1" == "2" ]; then
		echo AwaitHUPComplete | $TESTTOOL_DIR/diagtalker -pIMDIAG_PORT2 || error_exit  $?
	else
		echo AwaitHUPComplete | $TESTTOOL_DIR/diagtalker -p$IMDIAG_PORT || error_exit  $?
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
	interrupt_connection=NO
	if [ "$1" == "--interrupt-connection" ]; then
		interrupt_connection="YES"
		interrupt_host="$2"
		interrupt_port="$3"
		interrupt_tick="$4"
		shift 4
		lastcurrent_time=0
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

				current_time=$(date +%s)
				if [ $interrupt_connection == "YES" ] && [ $current_time -gt $lastcurrent_time ] && [ $((current_time % $interrupt_tick)) -eq 0 ] && [ ${count} -gt 1 ]; then
					# Interrupt Connection - requires root and linux kernel >= 4.9 in order to work!
					echo wait_file_lines Interrupt Connection on ${interrupt_host}:${interrupt_port}
					sudo ss -K dst ${interrupt_host} dport = ${interrupt_port}
				fi
				lastcurrent_time=$current_time

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
	printf 'HUP issued to pid %d - waiting for it to become processed\n' \
		$(cat $RSYSLOG_PIDBASE$1.pid)
	await_HUP_processed
	#$TESTTOOL_DIR/msleep $sleeptime
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
# Core Dump Handling (Issue #6268):
# - Test-specific cores (${RSYSLOG_DYNNAME}.core, core.${RSYSLOG_DYNNAME}.*)
#   are checked FIRST to prevent race conditions in parallel test runs
# - Generic cores are checked second for backward compatibility
# - Duplicate detection prevents processing the same core file twice
#
# Verbosity Reduction:
# - When no cores found: minimal output (just ulimit status)
# - When cores found: full analysis with backtraces
# - System info (uname, memory): only shown if cores found or VERBOSE mode
# - Disk space warning: only shown if usage >= 90% and no cores found
#
# NOTE: if a function test_error_exit_handler is defined, error_exit will
#       call it immediately before termination. This may be used to cleanup
#       some things or emit additional diagnostic information.
error_exit() {
	if [ $1 -eq $TB_ERR_TIMEOUT ]; then
		printf '%s Test %s exceeded max runtime of %d seconds\n' "$(tb_timestamp)" "$0" $TB_TEST_MAX_RUNTIME
	fi
	# Core dump analysis - always run when cores are detected
	core_dumps_found=0

	# Search for core dumps in multiple locations and patterns
	# Priority 1: Test-specific core files (prevents parallel test race condition)
	# Priority 2: Generic core files (for older systems or non-test-specific cores)

	# Process core files safely using while read to handle filenames with spaces/special chars
	process_core_file() {
		local corefile="$1"
		if [ -f "$corefile" ]; then
			core_dumps_found=$((core_dumps_found + 1))
			echo "=== Analyzing core dump #$core_dumps_found: $corefile ==="

			# Identify the core file type and size
			if command -v file >/dev/null 2>&1; then
			echo "Core file type:"
			file "$corefile"
		fi
		echo "Core file size: $(wc -c < "$corefile") bytes"

			# Use platform-appropriate debugger for stack trace
			if [ "$(uname)" == "Darwin" ]; then
				if command -v lldb >/dev/null 2>&1; then
					echo "Getting stack trace with lldb:"
					$SUDO lldb -c "$corefile" -b -o "bt all" -o "thread backtrace all" -o "quit" ../tools/rsyslogd || echo "lldb analysis failed"
					# Also try tcpflood if available
					if [ -x "./tcpflood" ]; then
						echo "Getting stack trace with lldb (tcpflood):"
						$SUDO lldb -c "$corefile" -b -o "bt all" -o "thread backtrace all" -o "quit" ./tcpflood || echo "lldb analysis failed for tcpflood"
					fi
				else
					echo "lldb not found, trying gdb..."
					_gdb_in=$(mktemp "${RSYSLOG_DYNNAME}.gdb.in.XXXXXX")
					echo "bt" > "$_gdb_in"
					echo "q" >> "$_gdb_in"
					$SUDO gdb ../tools/rsyslogd "$corefile" -batch -x "$_gdb_in" || echo "gdb analysis failed"
					# Also try tcpflood if available
					if [ -x "./tcpflood" ]; then
						$SUDO gdb ./tcpflood "$corefile" -batch -x "$_gdb_in" || echo "gdb analysis failed for tcpflood"
					fi
					rm -f "$_gdb_in"
				fi
			else
				_gdb_in=$(mktemp "${RSYSLOG_DYNNAME}.gdb.in.XXXXXX")
				echo "bt" > "$_gdb_in"
				echo "info thread" >> "$_gdb_in"
				echo "thread apply all bt full" >> "$_gdb_in"
				echo "q" >> "$_gdb_in"
				$SUDO gdb ../tools/rsyslogd "$corefile" -batch -x "$_gdb_in" || echo "gdb analysis failed"
				# Also try tcpflood if available
				if [ -x "./tcpflood" ]; then
					$SUDO gdb ./tcpflood "$corefile" -batch -x "$_gdb_in" || echo "gdb analysis failed for tcpflood"
				fi
				rm -f "$_gdb_in"
			fi
			echo ""
		fi
	}

	# Process glob patterns (handle no-match cleanly via nullglob)
	# Priority order: test-specific cores first to prevent race conditions in parallel tests
	shopt -q nullglob; _had_nullglob=$?
	[ $_had_nullglob -ne 0 ] && shopt -s nullglob
	
	# First, check for test-specific core files (prevents Issue #6268 race condition)
	if [ -n "$RSYSLOG_DYNNAME" ]; then
		for corefile in ${RSYSLOG_DYNNAME}.core core.${RSYSLOG_DYNNAME}.*; do
			process_core_file "$corefile"
		done
	fi
	
	# Then check generic patterns (for backward compatibility)
	for corefile in core* /cores/core-* /cores/core.*; do
		# Skip if already processed as test-specific
		if [ -n "$RSYSLOG_DYNNAME" ] && [[ "$corefile" == *"$RSYSLOG_DYNNAME"* ]]; then
			continue
		fi
		process_core_file "$corefile"
	done
	[ $_had_nullglob -ne 0 ] && shopt -u nullglob

	# Process find results safely using while read without subshell (process substitution)
	while IFS= read -r corefile; do
		# Skip if already processed
		if [ -n "$RSYSLOG_DYNNAME" ] && [[ "$corefile" == *"$RSYSLOG_DYNNAME"* ]]; then
			continue
		fi
		process_core_file "$corefile"
	done < <(find . -name "core.*" -o -name "*.core" 2>/dev/null)

	if [ $core_dumps_found -eq 0 ]; then
		# Reduced verbosity - only show essential information when no core found
		printf 'No core dumps found (ulimit -c: %s)\n' "$(ulimit -c)"
	else
		echo "=== Analyzed $core_dumps_found core dump(s) ==="
	fi

	# Check disk space only if it might be an issue
	if [ $core_dumps_found -eq 0 ]; then
		df -hP . | tail -1 | awk '{
			usage=$5+0;
			if (usage >= 90)
				print "WARNING: Disk usage is " $5 " (Free: " $4 ") - This may prevent core dump creation!"
		}'
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

	# Comprehensive system and error information gathering
	# Only show detailed system info if core dumps were found or verbose mode is on
	if [ $core_dumps_found -gt 0 ] || [ "${TEST_OUTPUT}" == "VERBOSE" ]; then
		echo "=== System Information ==="
		uname -a
		if [ "$(uname)" == "Darwin" ]; then
			if command -v sw_vers >/dev/null 2>&1; then
				sw_vers
			fi
			# macOS-specific memory information
			echo "=== macOS System Status ==="
			echo "Memory usage:"
			top -l 1 | grep "PhysMem" || echo "Memory info unavailable"

			# Recent system logs for rsyslogd (if available)
			echo "=== Recent macOS System Logs ==="
			if command -v log >/dev/null 2>&1; then
				log show --predicate 'process == "rsyslogd"' --info --last 1h || echo "No recent rsyslogd logs found"
			else
				echo "macOS log command not available"
			fi
		else
			# Linux-specific information
			echo "=== Linux System Status ==="
			if [ -f /proc/meminfo ]; then
				echo "Memory info:"
				grep -E "(MemTotal|MemFree|MemAvailable)" /proc/meminfo
			fi
		fi
	fi

	# Gather test error logs
	echo "=== Error Logs Collection ==="
	# Run gather-check-logs.sh if available; account for being invoked from tests/ dir
	if [ -f "$srcdir/../devtools/gather-check-logs.sh" ]; then
		echo "Running gather-check-logs.sh (via srcdir/..)..."
		( cd "$srcdir/.." && devtools/gather-check-logs.sh )
	elif [ -f "../devtools/gather-check-logs.sh" ]; then
		echo "Running gather-check-logs.sh (via ..)..."
		( cd .. && devtools/gather-check-logs.sh )
	elif [ -f "devtools/gather-check-logs.sh" ]; then
		echo "Running gather-check-logs.sh (cwd)..."
		devtools/gather-check-logs.sh
	else
		echo "gather-check-logs.sh not found, collecting available logs manually..."
	fi

	if [ -f "failed-tests.log" ]; then
		echo "=== Failed Tests Log ==="
		cat failed-tests.log
	else
		echo "No failed-tests.log found"
	fi

	# Extended debug output for dependencies started by testbench
	if [ "$EXTRA_EXITCHECK" == 'dumpkafkalogs' ] && [ "$TEST_OUTPUT" == "VERBOSE" ]; then
		# Dump Zookeeper log
		dump_zookeeper_serverlog
		# Dump Kafka log
		dump_kafka_serverlog
	fi

	# Extended Exit handling for kafka / zookeeper instances
	kafka_exit_handling "false"

	# Extended Exit handling for OTEL Collector instance
	otel_exit_handling "false"

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

	# Extended Exit handling for OTEL Collector instance
	otel_exit_handling "true"

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
		if command -v "$1" >/dev/null 2>&1; then
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
export RS_ZK_DOWNLOAD=apache-zookeeper-3.8.5-bin.tar.gz
dep_cache_dir=$(pwd)/.dep_cache
dep_zk_url=https://www.rsyslog.com/files/download/rsyslog/$RS_ZK_DOWNLOAD
dep_zk_cached_file=$dep_cache_dir/$RS_ZK_DOWNLOAD

export RS_KAFKA_DOWNLOAD=kafka_2.13-2.8.0.tgz
dep_kafka_url="https://www.rsyslog.com/files/download/rsyslog/$RS_KAFKA_DOWNLOAD"
dep_kafka_cached_file=$dep_cache_dir/$RS_KAFKA_DOWNLOAD

if [ -z "$OTEL_COLLECTOR_VERSION" ]; then
	export OTEL_COLLECTOR_VERSION="0.100.0"
fi
if [ -z "$OTEL_COLLECTOR_DOWNLOAD" ]; then
	# Detect architecture and OS for portable OTEL Collector downloads
	OTEL_ARCH=$(uname -m)
	OTEL_OS=$(uname -s | tr '[:upper:]' '[:lower:]')
	case "$OTEL_ARCH" in
		x86_64) OTEL_ARCH="amd64" ;;
		aarch64|arm64) OTEL_ARCH="arm64" ;;
		armv7l|armv6l) OTEL_ARCH="arm" ;;
		*) OTEL_ARCH="amd64" ;; # default fallback
	esac
	case "$OTEL_OS" in
		linux) OTEL_OS="linux" ;;
		darwin) OTEL_OS="darwin" ;;
		*) OTEL_OS="linux" ;; # default fallback
	esac
	export OTEL_COLLECTOR_DOWNLOAD="otelcol-contrib_${OTEL_COLLECTOR_VERSION}_${OTEL_OS}_${OTEL_ARCH}.tar.gz"
fi
dep_otel_collector_url="https://github.com/open-telemetry/opentelemetry-collector-releases/releases/download/v${OTEL_COLLECTOR_VERSION}/${OTEL_COLLECTOR_DOWNLOAD}"
dep_otel_collector_cached_file=$dep_cache_dir/$OTEL_COLLECTOR_DOWNLOAD

if [ -z "$ES_DOWNLOAD" ]; then
	export ES_DOWNLOAD=elasticsearch-7.14.1-linux-x86_64.tar.gz
fi
if [ -z "$ES_PORT" ]; then
	export ES_PORT=19200
fi
if [ -z "$ES_HOST" ]; then
	export ES_HOST=localhost
fi
if [ -n "$RSYSLOG_TESTBENCH_EXTERNAL_ES_URL" ]; then
	export RSYSLOG_TESTBENCH_EXTERNAL_ES_URL="${RSYSLOG_TESTBENCH_EXTERNAL_ES_URL%/}"
	export RSYSLOG_TESTBENCH_USE_EXTERNAL_ES="yes"
	es_url_no_proto="${RSYSLOG_TESTBENCH_EXTERNAL_ES_URL#*://}"
	if [ "$es_url_no_proto" = "$RSYSLOG_TESTBENCH_EXTERNAL_ES_URL" ]; then
		RSYSLOG_TESTBENCH_EXTERNAL_ES_URL="http://${RSYSLOG_TESTBENCH_EXTERNAL_ES_URL}"
		es_url_no_proto="${RSYSLOG_TESTBENCH_EXTERNAL_ES_URL#*://}"
	fi
	es_host_port="${es_url_no_proto%%/*}"
	if [ -n "$es_host_port" ]; then
		ES_HOST="${es_host_port%%:*}"
		if [ "$ES_HOST" = "$es_host_port" ]; then
			es_port=""
		else
			es_port="${es_host_port#*:}"
		fi
		if [ -n "$es_port" ]; then
			ES_PORT="$es_port"
		fi
	fi
fi

es_base_url() {
        local host="${ES_HOST:-localhost}"
        local port="${ES_PORT:-19200}"
        local proto="http"
        if [ -n "$RSYSLOG_TESTBENCH_EXTERNAL_ES_URL" ]; then
                proto="${RSYSLOG_TESTBENCH_EXTERNAL_ES_URL%%://*}"
                if [ "$proto" = "$RSYSLOG_TESTBENCH_EXTERNAL_ES_URL" ]; then
                        proto="http"
                fi
        fi
        printf '%s://%s:%s' "$proto" "$host" "$port"
}

es_detect_version() {
        local base
        base=$(es_base_url)

        local detect_url
        detect_url="${base%/}/"

        local payload
        if ! payload=$(curl --silent --show-error --connect-timeout 5 \
                --header 'Accept: application/json' "$detect_url"); then
                return 1
        fi

        local python_bin="${PYTHON:-}"
        if [ -n "$python_bin" ] && ! command -v "$python_bin" >/dev/null 2>&1; then
                python_bin=""
        fi
        if [ -z "$python_bin" ]; then
                if command -v python3 >/dev/null 2>&1; then
                        python_bin=$(command -v python3)
                elif command -v python >/dev/null 2>&1; then
                        python_bin=$(command -v python)
                fi
        fi
        if [ -z "$python_bin" ]; then
                printf 'info: unable to detect Elasticsearch version (no python interpreter available)\n'
                return 1
        fi

        local result
        if ! result=$(printf '%s' "$payload" | "$python_bin" -c '
import json
import sys

try:
    payload = json.load(sys.stdin)
except Exception:
    sys.exit(1)

version = payload.get("version", {})
number = version.get("number")

if not isinstance(number, str):
    sys.exit(1)

parts = number.split(".")
major = parts[0] if parts else ""

if not major.isdigit():
    sys.exit(1)

print(major)
print(number)
'); then
                return 1
        fi

        local detected_major=""
        local detected_full=""
        if [ -n "$result" ]; then
                IFS=$'\n' read -r detected_major detected_full <<EOF
$result
EOF
        fi

        if [ -z "$detected_major" ]; then
                return 1
        fi

        export RSYSLOG_TESTBENCH_ES_MAJOR_VERSION="$detected_major"
        if [ -n "$detected_full" ]; then
                export RSYSLOG_TESTBENCH_ES_VERSION_NUMBER="$detected_full"
        fi

        printf 'info: detected Elasticsearch version %s (major %s) at %s\n' \
                "${RSYSLOG_TESTBENCH_ES_VERSION_NUMBER:-unknown}" \
                "$RSYSLOG_TESTBENCH_ES_MAJOR_VERSION" \
                "$base"

        return 0
}

require_elasticsearch_restart_capability() {
        if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
                printf 'info: skipping test - external Elasticsearch instance cannot be restarted by the harness\n'
                exit 77
        fi
}

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

## otel_exit_handling - cleanup OTEL Collector instance on exit
## @param $1 - "true" or "false" indicating whether to force cleanup
otel_exit_handling() {
	if [[ "$EXTRA_EXIT" == 'otel' ]]; then
		echo "stop OTEL Collector instance"
		cleanup_otel_collector
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
			echo "Downloading zookeeper from $dep_zk_url"
			echo wget -q $dep_zk_url -O $dep_zk_cached_file
			wget -q $dep_zk_url -O $dep_zk_cached_file
			if [ $? -ne 0 ]
			then
                                echo error during wget, retry:
                                wget $dep_zk_url -O $dep_zk_cached_file
                                if [ $? -ne 0 ]
                                then
                                        echo "Skipping test - unable to download zookeeper"
                                        error_exit 77
                                fi
                        fi
                fi
        fi
	if [ ! -f $dep_kafka_cached_file ]; then
		if [ -f /local_dep_cache/$RS_KAFKA_DOWNLOAD ]; then
			printf 'Kafka: satisfying dependency %s from system cache.\n' "$RS_KAFKA_DOWNLOAD"
			cp /local_dep_cache/$RS_KAFKA_DOWNLOAD $dep_kafka_cached_file
		else
			echo "Downloading kafka from $dep_kafka_url"
			wget -q $dep_kafka_url -O $dep_kafka_cached_file
			if [ $? -ne 0 ]
			then
				echo error during wget, retry:
                                wget $dep_kafka_url -O $dep_kafka_cached_file
                                if [ $? -ne 0 ]
                                then
                                        rm $dep_kafka_cached_file # a 0-size file may be left over
                                        echo "Skipping test - unable to download kafka"
                                        error_exit 77
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
                        if [ -n "$kafkapid" ]; then
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

  if [ -z "$1" ]; then
      dep_work_dir=$(readlink -f ".dep_wrk")
      dep_work_tk_config="zoo.cfg"
  else
      dep_work_dir=$(readlink -f "$srcdir/$1")
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

        local zk_config="$dep_work_dir/zk/conf/zoo.cfg"
        local zk_client_port
        zk_client_port=$(awk -F= '/^[[:space:]]*clientPort[[:space:]]*=/ {gsub(/[[:space:]]*/, "", $2); print $2; exit}' "$zk_config")
        if [ -n "$zk_client_port" ]; then
                if ! wait_for_tcp_service "127.0.0.1" "$zk_client_port" 60 "zookeeper client port"; then
                        dump_zookeeper_serverlog "$1"
                        error_exit 77
                fi
        else
                printf 'WARNING: unable to determine ZooKeeper client port from %s\n' "$zk_config"
        fi
}

start_kafka() {
        printf '%s starting kafka\n' "$(tb_timestamp)"

        # Force IPv4 usage of Kafka!
        export KAFKA_OPTS="-Djava.net.preferIPv4Stack=True"
        export KAFKA_HEAP_OPTS="-Xms256m -Xmx256m" # we need to take care for smaller CI systems!
        local dep_work_dir
        local dep_work_kafka_config

        _kafka_instance_layout "$1" dep_work_dir dep_work_kafka_config

        # shellcheck disable=SC2009  - we do not grep on the process name!
        kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
        if [ "$KEEP_KAFKA_RUNNING" == "YES" ] && [ -n "$kafkapid" ]; then
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

        local kafka_dir="$dep_work_dir/kafka"
        local kafka_config_path="$kafka_dir/config/$dep_work_kafka_config"
        local readiness_ok=0

        if wait_for_kafka_ready_internal "$kafka_dir" "$kafka_config_path"; then
                readiness_ok=1
        fi

        # shellcheck disable=SC2009  - we do not grep on the process name!
        kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
        if [ "$readiness_ok" -eq 1 ] && [ -n "$kafkapid" ]; then
                echo "Kafka instance $dep_work_kafka_config (PID $kafkapid) started ... "
                return
        fi

        if [ -n "$kafkapid" ] && [ "$readiness_ok" -ne 1 ]; then
                echo "Kafka instance $dep_work_kafka_config has PID $kafkapid but readiness checks failed"
                echo "displaying all kafka logs now:"
                for logfile in $dep_work_dir/logs/*; do
                        echo "FILE: $logfile"
                        cat $logfile
                done
                error_exit 77
        fi

        echo "Starting Kafka instance $dep_work_kafka_config, SECOND ATTEMPT!"
        (cd $dep_work_dir/kafka && ./bin/kafka-server-start.sh -daemon ./config/$dep_work_kafka_config)

        if ! wait_for_kafka_ready_internal "$kafka_dir" "$kafka_config_path"; then
                echo "Failed to start Kafka instance for $dep_work_kafka_config"
                echo "displaying all kafka logs now:"
                for logfile in $dep_work_dir/logs/*; do
                        echo "FILE: $logfile"
                        cat $logfile
                done
                error_exit 77
        fi

        # shellcheck disable=SC2009  - we do not grep on the process name!
        kafkapid=$(ps aux | grep -i $dep_work_kafka_config | grep java | grep -v grep | awk '{print $2}')
        if [ -n "$kafkapid" ]; then
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
	if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
		return 0
	fi
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
	if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
		return 0
	fi
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
	printf '%s ensuring Elasticsearch ready\n' "$(tb_timestamp)"
        if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
                local base
                base=$(es_base_url)
                printf 'using external Elasticsearch instance at %s\n' "$base"
                if ! curl --silent --show-error --connect-timeout 5 "$base" >/dev/null; then
                        printf 'external Elasticsearch instance %s not reachable\n' "$base"
                        error_exit 77
                fi
                init_elasticsearch
                es_detect_version || printf 'info: unable to determine Elasticsearch version metadata\n'
                printf '\n%s Elasticsearch readied for use as requested\n' "$(date +%H:%M:%S)"
                return
	fi
	if   printf '%s:%s:%s\n' "$ES_DOWNLOAD" "$ES_PORT" "$(cat es.pid)" \
	   | cmp -b - elasticsearch.running
	then
		printf 'Elasticsearch already running, NOT restarting it\n'
	else
		printf '%s Elasticsearch not yet running, starting it\n' "$(date +%H:%M:%S)"
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
                es_detect_version || printf 'info: unable to determine Elasticsearch version metadata\n'
        fi
        printf '\n%s Elasticsearch readied for use as requested\n' "$(date +%H:%M:%S)"
}



# $2, if set, is the number of additional ES instances
start_elasticsearch() {
	if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
		printf 'info: skipping local Elasticsearch startup (external instance in use)\n'
		return 0
	fi
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
	local base
	base=$(es_base_url)
	# Loop until elasticsearch port is reachable or until
	# timeout is reached!
	curl --silent --show-error --connect-timeout 1 "$base"
	until [ "$(curl --silent --show-error --connect-timeout 1 "$base" | grep 'rsyslog-testbench')" != "" ]; do
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
	printf '%s getting data from ElasticSearch\n' "$(date +%H:%M:%S)"
	local base
	base=$(es_base_url)
	curl --silent -XPUT --show-error -H 'Content-Type: application/json' "${base}/rsyslog_testbench/_settings" -d '{ "index" : { "max_result_window" : '${1:-$NUMMESSAGES}' } }'
	# refresh to ensure we get the latest data
	curl --silent "${base}/rsyslog_testbench/_refresh"
	curl --silent "${base}/rsyslog_testbench/_search?size=${1:-$NUMMESSAGES}" > $RSYSLOG_DYNNAME.work
	printf '\n'
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
	if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
		printf 'info: skipping local Elasticsearch stop (external instance in use)\n'
		return 0
	fi
	dep_work_dir=$(readlink -f $srcdir)
	dep_work_es_pidfile="es.pid"
	rm -f elasticsearch.running
	if [ -e $dep_work_es_pidfile ]; then
		es_pid=$(cat $dep_work_es_pidfile)
		printf 'stopping ES with pid %d\n' $es_pid
		kill -SIGTERM $es_pid

		i=0
		while kill -0 $es_pid 2> /dev/null; do
			$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
			if test $i -gt $TB_TIMEOUT_STARTSTOP; then
				printf 'Elasticsearch (pid %d) still running - Performing hard shutdown (-9)\n' $es_pid
				kill -9 $es_pid
				break
			fi
			(( i++ ))
		done
	fi
}

# cleanup es leftovers when it is being stopped
cleanup_elasticsearch() {
		if [ "$RSYSLOG_TESTBENCH_USE_EXTERNAL_ES" = "yes" ]; then
			return 0
		fi
		dep_work_dir=$(readlink -f .dep_wrk)
		dep_work_es_pidfile="es.pid"
		stop_elasticsearch
		rm -f $dep_work_es_pidfile
		rm -rf $dep_work_dir/es
}

# initialize local Elasticsearch *testbench* instance for the next
# test. NOTE: do NOT put anything useful on that instance!
init_elasticsearch() {
	local base
	base=$(es_base_url)
	curl --silent -XDELETE "${base}/rsyslog_testbench"
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

omhttp_validate_metadata_response() {
	echo "starting to validate omhttp response metadata."
    omhttp_response_validate_py=$srcdir/omhttp-validate-response.py
    if [ ! -f $omhttp_response_validate_py ]; then
        echo "Cannot find ${omhttp_response_validate_py} for omhttp test"
        error_exit 1
    fi

	$PYTHON ${omhttp_response_validate_py} --error ${RSYSLOG_DYNNAME}/omhttp.error.log --response ${RSYSLOG_DYNNAME}/omhttp.response.log 2>&1
	if [ $? -ne 0 ] ; then
		printf 'omhttp_validate_metadata_response failed \n'
		error_exit 1
	fi
}

# download OTEL Collector binary from opentelemetry-collector-releases
download_otel_collector() {
	if [ ! -d $dep_cache_dir ]; then
		echo "Creating dependency cache dir $dep_cache_dir"
		mkdir -p $dep_cache_dir
	fi
	if [ ! -f $dep_otel_collector_cached_file ]; then
		if [ -f /local_dep_cache/$OTEL_COLLECTOR_DOWNLOAD ]; then
			printf 'OTEL Collector: satisfying dependency %s from system cache.\n' "$OTEL_COLLECTOR_DOWNLOAD"
			cp /local_dep_cache/$OTEL_COLLECTOR_DOWNLOAD $dep_otel_collector_cached_file
		else
			printf 'OTEL Collector: downloading %s from %s\n' "$OTEL_COLLECTOR_DOWNLOAD" "$dep_otel_collector_url"
			wget -q $dep_otel_collector_url -O $dep_otel_collector_cached_file
			if [ $? -ne 0 ]; then
				echo "error during wget, retry:"
				wget $dep_otel_collector_url -O $dep_otel_collector_cached_file
				if [ $? -ne 0 ]; then
					rm -f $dep_otel_collector_cached_file
					echo "Skipping test - unable to download OTEL Collector"
					error_exit 77
				fi
			fi
		fi
	fi
}

# prepare OTEL Collector instance for test
prepare_otel_collector() {
	# Ensure RSYSLOG_DYNNAME is set for per-test directory isolation
	if [ -z "$RSYSLOG_DYNNAME" ]; then
		echo "ERROR: RSYSLOG_DYNNAME is not set when preparing OTEL Collector"
		error_exit 1
	fi

	dep_work_otel_collector_config="otel-collector-config.yaml"
	dep_work_otel_collector_pidfile="otelcol.pid"

	# Create .dep_wrk directory first if it doesn't exist, then resolve path
	if [ ! -d .dep_wrk ]; then
		mkdir -p .dep_wrk
	fi
	dep_work_dir=$(readlink -f .dep_wrk 2>/dev/null || echo "$(pwd)/.dep_wrk")

	# Use per-test directory to allow parallel execution
	otelcol_work_dir="$dep_work_dir/otelcol-${RSYSLOG_DYNNAME}"

	if [ ! -f $dep_otel_collector_cached_file ]; then
		echo "Dependency-cache does not have OTEL Collector package, did you download dependencies?"
		error_exit 77
	fi
	# Clean up any existing instance for this test (not other tests)
	if [ -d "$otelcol_work_dir" ]; then
		if [ -e "$otelcol_work_dir/$dep_work_otel_collector_pidfile" ]; then
			otelcol_pid=$(cat "$otelcol_work_dir/$dep_work_otel_collector_pidfile")
			if kill -0 $otelcol_pid 2>/dev/null; then
				kill -SIGTERM $otelcol_pid 2>/dev/null
				wait_pid_termination $otelcol_pid
			fi
		fi
	fi
	if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
		$TESTTOOL_DIR/msleep 500
	else
		sleep 0.5
	fi
	rm -rf "$otelcol_work_dir"
	echo "TEST USES OTEL COLLECTOR BINARY $dep_otel_collector_cached_file"
	mkdir -p "$otelcol_work_dir"
	(cd "$otelcol_work_dir" && tar -zxf $dep_otel_collector_cached_file) > /dev/null

	# Find the actual binary location (tarball may extract to subdirectory or root)
	otelcol_binary=""
	if [ -f "$otelcol_work_dir/otelcol-contrib" ]; then
		otelcol_binary="$otelcol_work_dir/otelcol-contrib"
	elif [ -f "$otelcol_work_dir/otelcol-contrib/otelcol-contrib" ]; then
		otelcol_binary="$otelcol_work_dir/otelcol-contrib/otelcol-contrib"
		mv "$otelcol_work_dir/otelcol-contrib"/* "$otelcol_work_dir/" 2>/dev/null
		otelcol_binary="$otelcol_work_dir/otelcol-contrib"
	else
		# Try to find any otelcol-contrib binary
		otelcol_binary=$(find "$otelcol_work_dir" -name "otelcol-contrib" -type f | head -1)
		if [ -z "$otelcol_binary" ]; then
			echo "Could not find otelcol-contrib binary in extracted archive"
			error_exit 1
		fi
		# Move to root of otelcol directory
		otelcol_dir=$(dirname $otelcol_binary)
		if [ "$otelcol_dir" != "$otelcol_work_dir" ]; then
			mv $otelcol_dir/* "$otelcol_work_dir/" 2>/dev/null
			otelcol_binary="$otelcol_work_dir/otelcol-contrib"
		fi
	fi

	# Make binary executable
	chmod +x $otelcol_binary

	# Generate config file with dynamic port and output file path
	# Use absolute path so OTEL Collector writes to test directory regardless of working directory
	# Use srcdir if available (tests directory), otherwise use current directory
	test_dir="${srcdir:-$(pwd)}"
	# Ensure test_dir is absolute
	if [[ "$test_dir" != /* ]]; then
		test_dir="$(cd "$test_dir" && pwd)"
	fi
	otel_output_file="$test_dir/${RSYSLOG_DYNNAME}.otel-output.json"
	export OTEL_OUTPUT_FILE="$otel_output_file"

	# Ensure the output directory exists (OTEL Collector file exporter may not create it)
	mkdir -p "$(dirname "$otel_output_file")"

	# Get a free port for the collector (use existing get_free_port function for portability)
	if [ -z "$OTEL_COLLECTOR_PORT" ]; then
		OTEL_COLLECTOR_PORT=$(get_free_port)
	fi
	export OTEL_COLLECTOR_PORT

	# Get a free port for metrics/telemetry
	if [ -z "$OTEL_METRICS_PORT" ]; then
		OTEL_METRICS_PORT=$(get_free_port)
	fi
	export OTEL_METRICS_PORT

	if [ ! -f $srcdir/testsuites/$dep_work_otel_collector_config ]; then
		echo "OTEL Collector config template not found: $srcdir/testsuites/$dep_work_otel_collector_config"
		error_exit 1
	fi
	cp -f $srcdir/testsuites/$dep_work_otel_collector_config "$otelcol_work_dir/config.yaml"
	# Replace environment variable in config and set the port
	# Use absolute path - convert to absolute if relative
	if [[ "$otel_output_file" != /* ]]; then
		otel_output_file="$(cd "$(dirname "$otel_output_file")" && pwd)/$(basename "$otel_output_file")"
	fi
	# Ensure it's properly escaped for sed replacement string (only &, \, and delimiter need escaping)
	otel_output_file_escaped=$(printf '%s\n' "$otel_output_file" | sed -e 's/[&|\\]/\\&/g')
	sed -i "s|\${OTEL_OUTPUT_FILE}|$otel_output_file_escaped|g" "$otelcol_work_dir/config.yaml"
	sed -i "s|\${OTEL_METRICS_PORT}|$OTEL_METRICS_PORT|g" "$otelcol_work_dir/config.yaml"
	sed -i "s|endpoint: 0.0.0.0:0|endpoint: 0.0.0.0:$OTEL_COLLECTOR_PORT|g" "$otelcol_work_dir/config.yaml"

	if [ ! -f "$otelcol_work_dir/config.yaml" ]; then
		echo "Failed to create OTEL Collector config file"
		error_exit 1
	fi

	echo "OTEL Collector prepared for use in test."
	echo "OTEL Collector output file path: $otel_output_file"
	echo "OTEL Collector config:"
	cat "$otelcol_work_dir/config.yaml"
}

# start OTEL Collector and capture dynamic port
start_otel_collector() {
	# Use per-test directory to allow parallel execution
	if [ -z "$RSYSLOG_DYNNAME" ]; then
		echo "ERROR: RSYSLOG_DYNNAME is not set when starting OTEL Collector"
		error_exit 1
	fi
	dep_work_dir=$(readlink -f .dep_wrk 2>/dev/null || echo "$(pwd)/.dep_wrk")
	otelcol_work_dir="$dep_work_dir/otelcol-${RSYSLOG_DYNNAME}"
	dep_work_otel_collector_pidfile="$otelcol_work_dir/otelcol.pid"
	dep_work_otel_collector_logfile="$otelcol_work_dir/otelcol.log"
	otel_port_file="${RSYSLOG_DYNNAME}.otel_port.file"

	if [ ! -d "$otelcol_work_dir" ]; then
		echo "OTEL Collector work-dir $otelcol_work_dir does not exist, did you prepare it?"
		error_exit 1
	fi

	echo "Starting OTEL Collector"

	# Verify config file exists
	if [ ! -f "$otelcol_work_dir/config.yaml" ]; then
		echo "OTEL Collector config file not found: $otelcol_work_dir/config.yaml"
		error_exit 1
	fi

	# Binary should be at known location after prepare_otel_collector()
	otelcol_binary="$otelcol_work_dir/otelcol-contrib"
	if [ ! -f "$otelcol_binary" ]; then
		echo "ERROR: otelcol-contrib binary not found at $otelcol_binary"
		echo "Did you call prepare_otel_collector()?"
		error_exit 1
	fi

	# Verify binary is executable
	if [ ! -x "$otelcol_binary" ]; then
		chmod +x "$otelcol_binary"
	fi

	otelcol_binary_rel="./otelcol-contrib"

	# Start collector in background and capture output (both stdout and stderr)
	(cd "$otelcol_work_dir" && $otelcol_binary_rel --config=config.yaml > $dep_work_otel_collector_logfile 2>&1) &
	otelcol_pid=$!
	echo $otelcol_pid > $dep_work_otel_collector_pidfile

	# Wait a moment for the process to start
	if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
		$TESTTOOL_DIR/msleep 500
	else
		sleep 0.5
	fi

	# Use the port we configured (no discovery needed)
	otel_port="$OTEL_COLLECTOR_PORT"
	if [ -z "$otel_port" ]; then
		echo "ERROR: OTEL_COLLECTOR_PORT not set. Did you call prepare_otel_collector()?"
		error_exit 1
	fi
	echo $otel_port > $otel_port_file
	echo "OTEL Collector configured to listen on port $otel_port"

	# Wait a bit more for collector to be fully ready
	if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
		$TESTTOOL_DIR/msleep 1000
	else
		sleep 1
	fi

	# Verify port is listening using existing helper function
	if ! wait_for_tcp_service "127.0.0.1" "$otel_port" 10 "OTEL Collector"; then
		echo "OTEL Collector port $otel_port is not listening"
		if [ -f $dep_work_otel_collector_logfile ]; then
			echo "Dumping OTEL Collector log:"
			cat $dep_work_otel_collector_logfile
		fi
		kill $otelcol_pid 2>/dev/null
		error_exit 1
	fi

	printf 'OTEL Collector pid is %s, listening on port %s\n' "$otelcol_pid" "$otel_port"
}

# stop OTEL Collector gracefully
stop_otel_collector() {
	# Use per-test directory to allow parallel execution
	if [ -z "$RSYSLOG_DYNNAME" ]; then
		echo "ERROR: RSYSLOG_DYNNAME is not set when stopping OTEL Collector"
		return
	fi
	dep_work_dir=$(readlink -f .dep_wrk 2>/dev/null || echo "$(pwd)/.dep_wrk")
	otelcol_work_dir="$dep_work_dir/otelcol-${RSYSLOG_DYNNAME}"
	dep_work_otel_collector_pidfile="$otelcol_work_dir/otelcol.pid"

	if [ ! -f "$dep_work_otel_collector_pidfile" ]; then
		echo "OTEL Collector pidfile does not exist, no action needed"
		return
	fi

	otelcol_pid=$(cat "$dep_work_otel_collector_pidfile" 2>/dev/null)
	if [ -z "$otelcol_pid" ]; then
		echo "OTEL Collector pidfile is empty, no action needed"
		return
	fi

	# Check if process is still running
	if ! kill -0 $otelcol_pid 2>/dev/null; then
		echo "OTEL Collector process $otelcol_pid is not running"
		rm -f "$dep_work_otel_collector_pidfile"
		return
	fi

	echo "Stopping OTEL Collector (PID $otelcol_pid)"
	kill -SIGTERM $otelcol_pid

	# Wait for graceful shutdown
	i=0
	while kill -0 $otelcol_pid 2>/dev/null; do
		$TESTTOOL_DIR/msleep 100
		(( i++ ))
		if test $i -gt $TB_TIMEOUT_STARTSTOP; then
			echo "OTEL Collector (PID $otelcol_pid) still running - Performing hard shutdown (-9)"
			kill -9 $otelcol_pid 2>/dev/null
			break
		fi
	done

	rm -f "$dep_work_otel_collector_pidfile"
}

# cleanup OTEL Collector files
cleanup_otel_collector() {
	stop_otel_collector
	# Don't delete .dep_wrk/otelcol-${RSYSLOG_DYNNAME} on failure to allow inspection of output files
	# Only cleanup if test succeeded (check via RSYSLOG_TESTBENCH_TEST_STATUS if available)
	if [ "${RSYSLOG_TESTBENCH_SKIP_CLEANUP:-}" != "1" ]; then
		# Use per-test directory to allow parallel execution
		if [ -n "$RSYSLOG_DYNNAME" ]; then
			dep_work_dir=$(readlink -f .dep_wrk 2>/dev/null || echo "$(pwd)/.dep_wrk")
			otelcol_work_dir="$dep_work_dir/otelcol-${RSYSLOG_DYNNAME}"
			if [ -d "$otelcol_work_dir" ]; then
				echo "Cleanup OTEL Collector instance"
				rm -rf "$otelcol_work_dir"
			fi
		fi
	fi
}

# extract and format log records from OTEL Collector output file
otel_collector_get_data() {
	if [ -z "$OTEL_OUTPUT_FILE" ]; then
		echo "ERROR: OTEL_OUTPUT_FILE not set. Did you call prepare_otel_collector()?"
		error_exit 1
	fi

	otel_output_file="$OTEL_OUTPUT_FILE"

	# Wait for file to appear (OTEL Collector file exporter may buffer data)
	i=0
	timeout=10
	while [ $i -lt $timeout ] && [ ! -f "$otel_output_file" ]; do
		if [ -n "$TESTTOOL_DIR" ] && [ -f "$TESTTOOL_DIR/msleep" ]; then
			$TESTTOOL_DIR/msleep 500
		else
			sleep 0.5
		fi
		((i++))
	done

	if [ ! -f "$otel_output_file" ]; then
		echo "OTEL Collector output file does not exist: $otel_output_file"
		error_exit 1
	fi

	# Parse OTLP JSON structure and extract log records
	# Structure: resourceLogs[].scopeLogs[].logRecords[]
	# Extract body.stringValue from each log record
	$PYTHON -c "
import json
import sys

try:
    with open('$otel_output_file', 'r') as f:
        # OTLP file exporter writes one JSON object per line
        records = []
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
                # Navigate OTLP structure
                if 'resourceLogs' in data:
                    for resource_log in data['resourceLogs']:
                        if 'scopeLogs' in resource_log:
                            for scope_log in resource_log['scopeLogs']:
                                if 'logRecords' in scope_log:
                                    for log_record in scope_log['logRecords']:
                                        # Extract body string value
                                        body = log_record.get('body', {})
                                        body_value = None
                                        if 'stringValue' in body:
                                            body_value = body['stringValue']
                                        elif 'bytesValue' in body:
                                            # Handle bytes (base64 encoded)
                                            import base64
                                            try:
                                                body_value = base64.b64decode(body['bytesValue']).decode('utf-8', errors='ignore')
                                            except Exception:
                                                pass

                                        if body_value:
                                            # Extract just the numeric part for seq_check compatibility
                                            # chkseq expects just a number, not "msgnum:00000000"
                                            # Format is typically "msgnum:00000000" or "msgnum:00"
                                            if 'msgnum:' in body_value:
                                                # Extract numeric part after "msgnum:"
                                                # Handle both "msgnum:00000000" and "msgnum:00" formats
                                                num_part = body_value.split('msgnum:', 1)[1].strip()
                                                # Store as tuple (numeric_value, num_string) for proper sorting
                                                try:
                                                    num_value = int(num_part)
                                                    records.append((num_value, num_part))
                                                except ValueError:
                                                    # If not a valid number, try to extract digits
                                                    import re
                                                    digits = re.search(r'\d+', num_part)
                                                    if digits:
                                                        num_value = int(digits.group(0))
                                                        records.append((num_value, digits.group(0)))
                                                    else:
                                                        # Fallback: use a large number so it sorts last
                                                        records.append((999999999, body_value))
                                            else:
                                                # Try to extract number from the beginning of the string
                                                import re
                                                match = re.match(r'(\d+)', body_value)
                                                if match:
                                                    num_value = int(match.group(1))
                                                    records.append((num_value, match.group(1)))
                                                else:
                                                    # Fallback: use a large number so it sorts last
                                                    records.append((999999999, body_value))
            except json.JSONDecodeError:
                continue

        # Output records, one per line, sorted by numeric value
        # Extract just the numeric string (second element of tuple) for output
        for num_value, num_str in sorted(records):
            if num_str:
                print(num_str)
except Exception as e:
    sys.stderr.write('Error parsing OTEL output: {}\\n'.format(e))
    sys.exit(1)
" > ${RSYSLOG_OUT_LOG} 2>/dev/null
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

set_redis_tls() {
	# Only set a random TLS port if not set (useful when Redis must be restarted during a test)
	if [ -z "$REDIS_RANDOM_TLS_PORT" ]; then
		export REDIS_RANDOM_TLS_PORT="$(get_free_port)"
	fi
	redis_command "CONFIG SET tls-ca-cert-file $(pwd)/testsuites/x.509/ca.pem"
	redis_command "CONFIG SET tls-cert-file $(pwd)/testsuites/x.509/client-cert.pem"
	redis_command "CONFIG SET tls-key-file $(pwd)/testsuites/x.509/client-key.pem"
	redis_command "CONFIG SET tls-port ${REDIS_RANDOM_TLS_PORT}"
	redis_command "CONFIG REWRITE"
}

set_redis_tls_expired() {
	# Only set a random TLS port if not set (useful when Redis must be restarted during a test)
	if [ -z "$REDIS_RANDOM_TLS_PORT" ]; then
		export REDIS_RANDOM_TLS_PORT="$(get_free_port)"
	fi
	redis_command "CONFIG SET tls-ca-cert-file $(pwd)/testsuites/x.509/ca.pem"
	redis_command "CONFIG SET tls-cert-file $(pwd)/testsuites/x.509/client-expired-cert.pem"
	redis_command "CONFIG SET tls-key-file $(pwd)/testsuites/x.509/client-expired-key.pem"
	redis_command "CONFIG SET tls-port ${REDIS_RANDOM_TLS_PORT}"
	redis_command "CONFIG REWRITE"
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

	if [ -n "$REDIS_PASSWORD" ]; then
		printf "$1\n" | redis-cli --pass "$REDIS_PASSWORD" -p "$REDIS_RANDOM_PORT"
	else
		printf "$1\n" | redis-cli -p "$REDIS_RANDOM_PORT"
	fi
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
	SNMP_PYTHON=${SNMP_PYTHON:-$PYTHON}
    # Args: 1=port 2=outputfilename
    # Args 2 and up are passed along as is to snmptrapreceiver.py
    echo "Checking Python version..."
    if $SNMP_PYTHON -c 'import sys; print("Python version:", sys.version_info); exit(0 if sys.version_info >= (3,11) else 1)' 2>/dev/null; then
        echo "Python version > 3.10, using snmptrapreceiverv2.py"
        snmptrapreceiver=$srcdir/snmptrapreceiverv2.py
    else
        echo "Python version < 3.10, using snmptrapreceiver.py"
        snmptrapreceiver=$srcdir/snmptrapreceiver.py
    fi
    if [ ! -f ${snmptrapreceiver} ]; then
        echo "Cannot find ${snmptrapreceiver} for omsnmp test"
        error_exit 1
    fi

    # Test if the script can be executed at all
    echo "Testing Python script execution..."
    if ! $SNMP_PYTHON ${snmptrapreceiver} --help >/dev/null 2>&1; then
        echo "Warning: Python script does not support --help, testing basic execution..."
        if ! $SNMP_PYTHON -c "import sys; print('Python executable test passed')" >/dev/null 2>&1; then
            echo "ERROR: Python executable test failed"
            error_exit 1
        fi
    fi

    # Test if required Python packages are available
    echo "Testing required Python packages..."
    if ! $SNMP_PYTHON -c "import pysnmp; print('pysnmp available')" >/dev/null 2>&1; then
        echo "ERROR: pysnmp package not available"
        error_exit 1
    fi
    if ! $SNMP_PYTHON -c "import pyasn1; print('pyasn1 available')" >/dev/null 2>&1; then
        echo "ERROR: pyasn1 package not available"
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

    # Check if port is already in use
    echo "Checking if port ${snmp_server_port} is available..."
    if netstat -tuln 2>/dev/null | grep -q ":${snmp_server_port} "; then
        echo "WARNING: Port ${snmp_server_port} appears to be in use"
        echo "Active connections on port ${snmp_server_port}:"
        netstat -tuln 2>/dev/null | grep ":${snmp_server_port} " || true
    fi

    # Create work directory for parallel tests
    snmp_work_dir=${RSYSLOG_DYNNAME}/snmptrapreceiver

    snmp_server_pidfile="${snmp_work_dir}/snmp_server.pid"
    snmp_server_logfile="${snmp_work_dir}/snmp_server.log"
    mkdir -p ${snmp_work_dir}

    server_args="${snmp_server_port} 127.0.0.1 ${output_file}"
    echo "RUN SNMP SERVER WITH: $SNMP_PYTHON ${snmptrapreceiver} ${server_args} ${snmp_server_logfile} >> ${snmp_server_logfile} 2>&1"
    $SNMP_PYTHON ${snmptrapreceiver} ${server_args} ${snmp_server_logfile} >> ${snmp_server_logfile} 2>&1 &
    snmp_server_pid=$!
    if ! ps -p $snmp_server_pid > /dev/null 2>&1; then
        echo "Failed to start snmptrapreceiver with $SNMP_PYTHON."
        echo "Debug: Checking if Python script exists and is executable..."
        ls -la ${snmptrapreceiver}
        echo "Debug: Testing Python script execution..."
        $SNMP_PYTHON ${snmptrapreceiver} --help 2>&1 || echo "Script execution test failed"
        if [ "$SNMP_PYTHON" = "/usr/bin/python" ]; then
            SNMP_PYTHON="/usr/bin/python3"
			rm -rf ${snmp_work_dir}
		    mkdir -p ${snmp_work_dir}
            echo "Retrying with python3..."
		    echo "RUN SNMP SERVER WITH: $SNMP_PYTHON ${snmptrapreceiver} ${server_args} ${snmp_server_logfile} >> ${snmp_server_logfile} 2>&1"
            $SNMP_PYTHON ${snmptrapreceiver} ${server_args} ${snmp_server_logfile} >> ${snmp_server_logfile} 2>&1 &
			snmp_server_pid=$!
			if ! ps -p $snmp_server_pid > /dev/null 2>&1; then
                echo "Failed to start snmptrapreceiver with $SNMP_PYTHON."
                echo "Debug: Checking log file for errors..."
                if [ -f "${snmp_server_logfile}" ]; then
                    echo "SNMP server log content:"
                    cat "${snmp_server_logfile}"
                fi
				rm -rf ${snmp_work_dir}
                error_exit 1
            fi
        fi
    fi
    echo ${snmp_server_pid} > ${snmp_server_pidfile}

    # Wait for .started file with 30 second timeout
    echo "Waiting for SNMP server to create .started file..."
    timeout_start=$(date +%s)
    timeout_duration=30
    while test ! -f ${snmp_server_logfile}.started; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		printf "."
		current_time=$(date +%s)
		if [ $((current_time - timeout_start)) -gt $timeout_duration ]; then
		    echo ""
		    echo "TIMEOUT: SNMP server failed to create .started file within ${timeout_duration} seconds"
		    echo "Debug: Checking if process is still running..."
		    if ps -p $snmp_server_pid > /dev/null 2>&1; then
		        echo "Process is still running, checking log file..."
		        if [ -f "${snmp_server_logfile}" ]; then
		            echo "SNMP server log content:"
		            cat "${snmp_server_logfile}"
		        fi
		    else
		        echo "Process is no longer running"
		        if [ -f "${snmp_server_logfile}" ]; then
		            echo "SNMP server log content:"
		            cat "${snmp_server_logfile}"
		        fi
		    fi
		    snmp_stop_trapreceiver
		    error_exit 1
		fi
	done
    echo ""

    # Wait for log file to have content with 30 second timeout
    echo "Waiting for SNMP server log file to have content..."
    timeout_start=$(date +%s)
    timeout_duration=30
    while test ! -s "${snmp_server_logfile}"; do
		$TESTTOOL_DIR/msleep 100 # wait 100 milliseconds
		current_time=$(date +%s)
		if [ $((current_time - timeout_start)) -gt $timeout_duration ]; then
		    echo "TIMEOUT: SNMP server log file remained empty for ${timeout_duration} seconds"
		    echo "Debug: Checking if process is still running..."
		    if ps -p $snmp_server_pid > /dev/null 2>&1; then
		        echo "Process is still running, checking log file..."
		        if [ -f "${snmp_server_logfile}" ]; then
		            echo "SNMP server log content:"
		            cat "${snmp_server_logfile}"
		        fi
		    else
		        echo "Process is no longer running"
		        if [ -f "${snmp_server_logfile}" ]; then
		            echo "SNMP server log content:"
		            cat "${snmp_server_logfile}"
		        fi
		    fi
		    snmp_stop_trapreceiver
		    error_exit 1
		fi
    done

    echo "Started snmptrapreceiver with ${SNMP_PYTHON} args ${server_args} with pid ${snmp_server_pid}"
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
	echo "SNMP Trap Receiver log:"
	cat ${snmp_work_dir}//snmp_server.log
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

## Start the helper SNI server for omfwd tests.
## Args: 1=library (openssl|gnutls), 2=port
omfwd_sni_server() {
	"./$1_sni_server" "$2" "$srcdir/tls-certs/cert.pem" "$srcdir/tls-certs/key.pem" \
		1>"$RSYSLOG_DYNNAME.sni-server.stdout" &
	echo "$!" >"$RSYSLOG_DYNNAME.sni-server.pid"
}

## Validate that the SNI server observed the expected name.
## Args: 1=sni
omfwd_sni_check() {
	sni=${1}

	wait_file_lines "$RSYSLOG_DYNNAME.sni-server.stdout" 1

	if ! grep -q "^SNI: $sni\$" $RSYSLOG_DYNNAME.sni-server.stdout; then
	    echo "Expected 'SNI: $sni', but got '"`cat $RSYSLOG_DYNNAME.sni-server.stdout`"'"
		error_exit 1
	fi
}

case $1 in
  'init')
    export srcdir # in case it was not yet in environment
    $srcdir/killrsyslog.sh # kill rsyslogd if it runs for some reason
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
		# Generate a short ASCII-only random suffix in a POSIX/portable way.
		# On macOS, BSD tr with UTF-8 locales can error with "Illegal byte sequence"
		# when fed /dev/urandom. Force C locale and use head -c (portable) instead of
		# GNU head --bytes to avoid flaky failures in GitHub Actions runners.
		export RSYSLOG_DYNNAME="rstb_$(./test_id $(basename $0))$(LC_ALL=C tr -dc 'a-zA-Z0-9' < /dev/urandom | head -c 4)"
		export RSYSLOG_OUT_LOG="${RSYSLOG_DYNNAME}.out.log"
		export RSYSLOG2_OUT_LOG="${RSYSLOG_DYNNAME}_2.out.log"
		export RSYSLOG_PIDBASE="${RSYSLOG_DYNNAME}:" # also used by instance 2!

		# ensure test tools exist when running tests directly
		if [ ! -x "${TESTTOOL_DIR}/tcpflood" ]; then
			echo 'Building test tools...'
			# build all test tools via "make check" - configure knows which tools
			# to enable based on detected libraries, and TESTS="" ensures only
			# compilation (no test execution). This is more robust than ad hoc
			# builds because it honors configure-time feature detection.
make -j$(getconf _NPROCESSORS_ONLN) check TESTS="" || error_exit 100
		fi

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
		# Note: Setting per-process core_pattern would require root privileges via
		# /proc/sys/kernel/core_pattern, which is not available in standard test environments.
		# Instead, we detect test-specific cores using RSYSLOG_DYNNAME pattern matching
		# in error_exit() to prevent race conditions in parallel test runs (Issue #6268).
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
		# Also clean up any test-specific core files from previous runs
		if [ -n "$RSYSLOG_DYNNAME" ]; then
			rm -f ${RSYSLOG_DYNNAME}.core core.${RSYSLOG_DYNNAME}.*
		fi
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

   'wait-kafka-startup')
                wait_for_kafka_startup "$2" "$3"
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
