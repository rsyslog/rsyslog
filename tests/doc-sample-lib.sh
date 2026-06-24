#!/bin/bash
# Shared helper for documentation example tests. These tests copy user-facing
# RainerScript snippets into generated configs and use rsyslogd -C -N1 as the
# oracle so doc examples keep passing config validation.

doc_sample_expect_config_valid() {
	local label="$1"
	local cfg="$2"
	local log="${RSYSLOG_DYNNAME}.${label}.log"

	../tools/rsyslogd -C -N1 -M"${RSYSLOG_MODDIR}" -f"${cfg}" >"${log}" 2>&1
	local rc=$?
	if [ $rc -ne 0 ]; then
		echo "FAIL: expected documentation sample ${label} to validate"
		echo "Generated config:"
		cat -n "${cfg}"
		echo "rsyslogd output:"
		cat "${log}"
		error_exit 1
	fi
}

doc_sample_require_loadable_module() {
	local module_path="$1"
	local module_name="$2"

	if ! command -v ldd >/dev/null 2>&1; then
		return 0
	fi

	if ldd "${module_path}" 2>/dev/null | grep -q 'not found'; then
		echo "${module_name}: unresolved shared-library dependency for ${module_path}, skipping"
		exit 77
	fi
}
