#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Parse sender ACLs through both native frontends without starting listeners.
# Strict -N1 status is the oracle, not permissive startup's ignored-error path.
# Numeric and wildcard forms are legitimate controls; unresolved candidate
# policy is covered separately after global options have finalized.
# Malformed masks, extra tokens and empty arrays must never become allow-all.
. ${srcdir:=.}/diag.sh init
require_yaml_support
require_plugin imtcp
for frontend in rainerscript yaml; do
	for policy in '["127.0.0.1/32"]' '["[::1]/128"]' '["*.example.invalid"]' '[]' '["::1/128"]' '["[::1/128"]' '["127.0.0.1/99"]' '["127.0.0.1/32 extra"]'; do
		if [[ "$frontend" == yaml ]]; then
			generate_conf --yaml-only
			add_yaml_conf 'modules:'
			add_yaml_conf '  - load: "../plugins/imtcp/.libs/imtcp"'
			add_yaml_conf '    allowedSender: '"$policy"
			add_yaml_conf 'inputs:'
			add_yaml_conf '  - type: imtcp'
			add_yaml_conf '    port: "0"'
			add_yaml_conf '    ruleset: main'
			add_yaml_conf 'rulesets:'
			add_yaml_conf '  - name: main'
			add_yaml_conf '    actions:'
			add_yaml_conf '      - type: omfile'
			add_yaml_conf '        file: "'$RSYSLOG_OUT_LOG'"'
			config="${TESTCONF_NM}.yaml"
			sed -i '/debug.abortOnProgramError:/a\  net.aclResolveHostname: "off"' "$config"
		else
			generate_conf
			add_conf 'global(net.aclResolveHostname="off")'
			add_conf 'module(load="../plugins/imtcp/.libs/imtcp" allowedSender='"$policy"')'
			add_conf 'input(type="imtcp" port="0")'
			add_conf 'action(type="omfile" file="'$RSYSLOG_OUT_LOG'")'
			config="${TESTCONF_NM}.conf"
		fi
		log="$RSYSLOG_DYNNAME.$frontend.validation"
		../tools/rsyslogd -C -N1 -f"$config" -M"$RSYSLOG_MODDIR" >"$log" 2>&1
		status=$?
		case "$policy" in
			'[]'|'["::1/128"]'|'["[::1/128"]'|'["127.0.0.1/99"]'|'["127.0.0.1/32 extra"]')
				[[ "$status" -gt 0 && "$status" -lt 128 ]] || error_exit 1 "malformed $frontend ACL was accepted: $policy"
				;;
			*)
				[[ "$status" -eq 0 ]] || { cat "$log"; error_exit 1 "valid $frontend ACL rejected: $policy"; }
				;;
		esac
	done
done
exit_test
