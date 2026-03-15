#!/bin/bash

omazuredce_require_env() {
	if [[ -z "${AZURE_DCE_CLIENT_ID}" ]]; then
		echo "SKIP: AZURE_DCE_CLIENT_ID environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AZURE_DCE_CLIENT_SECRET}" ]]; then
		echo "SKIP: AZURE_DCE_CLIENT_SECRET environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AZURE_DCE_TENANT_ID}" ]]; then
		echo "SKIP: AZURE_DCE_TENANT_ID environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AZURE_DCE_URL}" ]]; then
		echo "SKIP: AZURE_DCE_URL environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AZURE_DCE_DCR_ID}" ]]; then
		echo "SKIP: AZURE_DCE_DCR_ID environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AZURE_DCE_TABLE_NAME}" ]]; then
		echo "SKIP: AZURE_DCE_TABLE_NAME environment variable not set - SKIPPING"
		exit 77
	fi
}

omazuredce_url_with_trailing_slash() {
	if [[ "${AZURE_DCE_URL}" == */ ]]; then
		printf '%s' "${AZURE_DCE_URL}"
	else
		printf '%s/' "${AZURE_DCE_URL}"
	fi
}

omazuredce_url_host() {
	local rest

	rest=${AZURE_DCE_URL#*://}
	rest=${rest%%/*}
	if [[ "${rest}" == \[*\] ]]; then
		printf '%s\n' "${rest#\[}" | sed 's/\]$//'
	elif [[ "${rest}" == \[*\]:* ]]; then
		rest=${rest#\[}
		printf '%s\n' "${rest%%]*}"
	else
		printf '%s\n' "${rest%%:*}"
	fi
}

omazuredce_url_port() {
	local rest explicit_port

	rest=${AZURE_DCE_URL#*://}
	rest=${rest%%/*}
	if [[ "${rest}" == \[*\]:* ]]; then
		explicit_port=${rest##*:}
	elif [[ "${rest}" == \[*\] ]]; then
		explicit_port=
	else
		explicit_port=${rest##*:}
		if [[ "${explicit_port}" == "${rest}" ]]; then
			explicit_port=
		fi
	fi
	if [[ -n "${explicit_port}" ]]; then
		printf '%s\n' "${explicit_port}"
	elif [[ "${AZURE_DCE_URL}" == https://* ]]; then
		printf '443\n'
	else
		printf '80\n'
	fi
}

omazuredce_url_ips_v4() {
	local host
	host=$(omazuredce_url_host)

	if [[ "${host}" == *:* ]]; then
		return
	fi
	if command -v getent >/dev/null 2>&1; then
		getent ahostsv4 "$host" | awk 'NR == 1 { print $1; exit }'
	fi
}

omazuredce_url_ips_v6() {
	local host
	host=$(omazuredce_url_host)

	if [[ "${host}" == *:* ]]; then
		printf '%s\n' "${host}"
		return
	fi
	if command -v getent >/dev/null 2>&1; then
		getent ahostsv6 "$host" | awk 'NR == 1 { print $1; exit }'
	fi
}
