#!/bin/bash
# added 2016-04-15 by Thomas D., released under ASL 2.0
# Several tests need another user/group to test impersonation.
# This script can be sourced to prevent duplicated code.

# To support <bash-4.2 which don't support "declare -g" we declare
# the array outside of the function
# shellcheck disable=SC2034
declare -A TESTBENCH_TESTUSER

rsyslog_testbench_setup_testuser() {
	local require_user_access="${1:-}"
	local has_testuser=
	local testusername=
	local testgroupname=

	if [ -z "${EUID}" ]; then
		# Should never happen
		echo "FATAL ERROR: \$EUID not set!"
		exit 1
	fi

	if [ ${EUID} -eq 0 ]; then
		# Only root is able to become a different user

		local testusers=("rsyslog" "syslog" "daemon")

		if [ -n "${RSYSLOG_TESTUSER}" ]; then
			# User has specified an username/uid we should use in testbench
			testusers=("${RSYSLOG_TESTUSER}" "${testusers[@]}")
		fi

		local testuser=
		for testuser in "${testusers[@]}"; do
			testusername=$(id --user --name "${testuser}" 2>/dev/null)
			if [ -z "${testusername}" ]; then
				echo "'id' did not find user \"${testuser}\" ... skipping, trying next user!"
				continue
			fi

			testgroupname=$(id --group --name "${testuser}" 2>/dev/null)
			if [ -z "${testgroupname}" ]; then
				echo "'id' did not find a primary group for \"${testuser}\" ... skipping, trying next user!"
				continue
			fi

			has_testuser="${testuser}"
			break
		done
		if [ -z "${has_testuser}" ]; then
			echo "ERROR: running as root and no suiteable testuser found - skipping test"
			echo 'You mas set a testuser via the RSYSLOG_TESTUSER environment variable'
			exit 77
		fi
		echo "WARNING: making work directory world-writable, as we need this to be able to"
		echo "         open and process files after privilege drop. This is NOT automatically"
		echo "         undone."
		chmod a+w .

		# The daemon loads some transport modules lazily after dropping privileges.
		# Skip when the selected user cannot traverse the build path rather than
		# timing out while waiting for imdiag to create its listener port file.
		if [ "${require_user_access}" = "require-user-access" ] && command -v runuser >/dev/null 2>&1; then
			local can_access_module_dir=0
			local module_dir
			local resolved_module_dir
			local module_dirs=()
			IFS=: read -r -a module_dirs <<< "${RSYSLOG_MODDIR}"
			for module_dir in "${module_dirs[@]}"; do
				resolved_module_dir=$(cd "${module_dir}" 2>/dev/null && pwd -P) || continue
				if runuser -u "${testusername}" -- test -x "${resolved_module_dir}" &&
				   runuser -u "${testusername}" -- test -r "${resolved_module_dir}"; then
					can_access_module_dir=1
					break
				fi
			done
			if [ "${can_access_module_dir}" -ne 1 ]; then
				echo "Skipping: test user '${testusername}' cannot access the build or test directory after privilege drop"
				exit 77
			fi
		fi
	fi

	if [ -z "${has_testuser}" ]; then
		testgroupname=$(id --group --name ${EUID} 2>/dev/null)
		if [ -z "${testgroupname}" ]; then
			echo "Skipping ... please set RSYSLOG_TESTUSER or make sure the user running the testbench has a primary group!"
			exit_test
			exit 0
		else
			has_testuser="${EUID}"
		fi
	fi

	_rsyslog_testbench_declare_testuser ${has_testuser}
}

_rsyslog_testbench_declare_testuser() {
	local testuser=$1

	local testusername
	testusername=$(id --user --name "${testuser}" 2>/dev/null)
	if [ -z "${testusername}" ]; then
		# Should never happen
		echo "FATAL ERROR: Could not get username for user \"${testuser}\"!"
		exit 1
	fi

	local testuid
	testuid=$(id --user "${testuser}" 2>/dev/null)
	if [ -z "${testuid}" ]; then
		# Should never happen
		echo "FATAL ERROR: Could not get uid for user \"${testuser}\"!"
		exit 1
	fi

	local testgroupname
	testgroupname=$(id --group --name "${testuser}" 2>/dev/null)
	if [ -z "${testgroupname}" ]; then
		# Should never happen
		echo "FATAL ERROR: Could not get uid of user \"${testuser}\"!"
		exit 1
	fi

	local testgid
	testgid=$(id --group "${testuser}" 2>/dev/null)
	if [ -z "${testgid}" ]; then
		# Should never happen
		echo "FATAL ERROR: Could not get primary gid of user \"${testuser}\"!"
		exit 1
	fi

	echo "Will use user \"${testusername}\" (#${testuid}) and group \"${testgroupname}\" (#${testgid})"

	TESTBENCH_TESTUSER[username]=${testusername}
	TESTBENCH_TESTUSER[uid]=${testuid}
	TESTBENCH_TESTUSER[groupname]=${testgroupname}
	TESTBENCH_TESTUSER[gid]=${testgid}
}
