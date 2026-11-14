#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Verify the shared fileCreateMode parser accepts exactly four or five octal
# digits beginning with zero and rejects invalid digits and trailing suffixes.
# The oracle is rsyslogd -N1 plus the parser's file-mode diagnostic; no output
# file is opened and no runtime timing is involved. Exercise module and action
# parameters because both use eCmdHdlrFileCreateMode.
. ${srcdir:=.}/diag.sh init

modpath="../runtime/.libs:../.libs"

write_config() {
	local cfg="$1"
	local module_mode="$2"
	local action_mode="$3"

	cat >"${cfg}" <<CONF_EOF
module(load="builtin:omfile" fileCreateMode="${module_mode}")
action(type="omfile" file="${RSYSLOG_DYNNAME}.out" fileCreateMode="${action_mode}")
CONF_EOF
}

run_valid_config() {
	local name="$1"
	local module_mode="$2"
	local action_mode="$3"
	local cfg="${RSYSLOG_DYNNAME}.${name}.conf"
	local log="${RSYSLOG_DYNNAME}.${name}.log"

	write_config "$cfg" "$module_mode" "$action_mode"
	../tools/rsyslogd -N1 -f"${cfg}" -M"${modpath}" >"${log}" 2>&1
	if [ $? -ne 0 ]; then
		echo "FAIL: expected fileCreateMode module=${module_mode} action=${action_mode} to validate"
		cat "${log}"
		error_exit 1
	fi
}

run_invalid_config() {
	local name="$1"
	local module_mode="$2"
	local action_mode="$3"
	local cfg="${RSYSLOG_DYNNAME}.${name}.conf"
	local log="${RSYSLOG_DYNNAME}.${name}.log"

	write_config "$cfg" "$module_mode" "$action_mode"
	../tools/rsyslogd -N1 -f"${cfg}" -M"${modpath}" >"${log}" 2>&1
	if [ $? -eq 0 ]; then
		echo "FAIL: expected fileCreateMode module=${module_mode} action=${action_mode} to fail validation"
		cat "${log}"
		error_exit 1
	fi
	content_check "file modes need to be specified as" "${log}"
}

run_valid_config "valid-four" "0644" "0644"
run_valid_config "valid-five" "04777" "04777"
run_invalid_config "invalid-module-octal" "0648" "0644"
run_invalid_config "invalid-action-octal" "0644" "0648"
run_invalid_config "invalid-five-digit" "0644" "0644x"
run_invalid_config "invalid-module-suffix" "0644oops" "0644"
run_invalid_config "invalid-action-suffix" "0644" "0644oops"

exit_test
