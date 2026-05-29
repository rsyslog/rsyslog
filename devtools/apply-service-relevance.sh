#!/usr/bin/env bash
# Apply PR-style service relevance gates to local or CI container configure
# options. Source this file before devtools/run-ci.sh so the exported
# RSYSLOG_CONFIGURE_OPTIONS_EXTRA value remains visible to the caller.

rsyslog_service_family_needs_testing() {
	local family="$1"

	if tests/diag.sh module-needs-testing "$family"; then
		return 0
	fi

	return 1
}

rsyslog_add_service_test_suppression() {
	local family="$1"
	local disable_opt="$2"

	case " ${RSYSLOG_CONFIGURE_OPTIONS:-} ${RSYSLOG_CONFIGURE_OPTIONS_EXTRA:-} ${RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE:-} " in
	*" $disable_opt "*)
		return 0
		;;
	esac

	if rsyslog_service_family_needs_testing "$family"; then
		echo "info: service relevance: keeping $family tests enabled"
	else
		echo "info: service relevance: adding $disable_opt for irrelevant $family tests"
		RSYSLOG_CONFIGURE_OPTIONS_EXTRA="${RSYSLOG_CONFIGURE_OPTIONS_EXTRA:+$RSYSLOG_CONFIGURE_OPTIONS_EXTRA }$disable_opt"
		export RSYSLOG_CONFIGURE_OPTIONS_EXTRA
	fi
}

rsyslog_apply_default_pr_service_suppressions() {
	rsyslog_add_service_test_suppression clickhouse --disable-clickhouse-tests
	rsyslog_add_service_test_suppression pgsql --disable-pgsql-tests
	rsyslog_add_service_test_suppression hiredis --disable-redis-tests
	rsyslog_add_service_test_suppression imdocker --disable-imdocker-tests
	rsyslog_add_service_test_suppression imfile --disable-imfile-tests
	rsyslog_add_service_test_suppression kafka --disable-kafka-tests
	rsyslog_add_service_test_suppression imbeats --disable-imbeats
	rsyslog_add_service_test_suppression omrabbitmq --disable-omrabbitmq
	rsyslog_add_service_test_suppression journal --disable-journal-tests
	rsyslog_add_service_test_suppression snmp --disable-snmp-tests
	rsyslog_add_service_test_suppression omazureeventhubs --disable-omazureeventhubs-tests
	rsyslog_add_service_test_suppression omazuredce --disable-omazuredce-tests
	rsyslog_add_service_test_suppression impstats-push --disable-impstats-push
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
	echo "error: source this script so exported configure options affect the caller" >&2
	exit 2
fi
