#!/bin/bash
# Exercise the experimental local queue's fail-closed activation contract,
# including queued actions, bounded retry, and both DA engines and S6 logical policies.
# Valid configurations must pass -N1; each invalid case must exit with the
# dedicated local-config error in -N1, partial validation, and ordinary startup
# even when AbortOnUncleanConfig is off. Stderr is intentional: rejection must
# precede usable queues. The 20s timeout only detects accidental startup/hangs;
# elapsed time is never a success oracle. Optional module cases run only when
# the module was built. Valid modules use -N1, so no listener or helper runs.
# Numeric cases exercise raw domains before narrowing; the global legacy
# control must preserve the historical behavior.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
if [ "${LOCAL_QUEUE_CONFIG_YAML:-0}" -eq 1 ]; then
    require_yaml_support
fi
# The compact parameter map needs Bash associative arrays. macOS still ships
# Bash 3, where continuing would turn dotted queue keys into arithmetic syntax.
if [ "${BASH_VERSINFO[0]}" -lt 4 ]; then
    echo 'SKIP local queue configuration matrix requires Bash 4 associative arrays'
    exit 77
fi
timeout_cmd=${timeout_cmd:-timeout}
command -v "$timeout_cmd" >/dev/null 2>&1 || timeout_cmd=gtimeout
command -v "$timeout_cmd" >/dev/null 2>&1 || {
    echo 'Testbench requires unavailable command: timeout or gtimeout'
    exit 77
}

write_config() {
    local scenario="$1" key
    local action_extra="" statement="" preamble="" global_extra=""
    local module_name="" module_rs="" module_yaml="" input_type=""
    local extra_ruleset="" duplicate_main=0
    local -A params=(
        [queue.scope]=local [queue.type]=FixedArray [queue.size]=1000
        [queue.local.frontendsize]=100 [queue.local.maxfrontends]=2
    )
    case "$scenario" in
        valid) ;;
        valid-helper-default) params[queue.dequeuebatchsize]=64 ;;
        valid-helper-zero) params[queue.local.helperbatchsize]=0 ;;
        valid-helper-cap) params[queue.local.helperbatchsize]=64; params[queue.dequeuebatchsize]=64 ;;
        helper-negative) params[queue.local.helperbatchsize]=-1 ;;
        helper-above-cap) params[queue.local.helperbatchsize]=65; params[queue.dequeuebatchsize]=64 ;;
        helper-overflow) params[queue.local.helperbatchsize]=4294967296 ;;
        helper-global)
            params[queue.scope]=global; params[queue.local.helperbatchsize]=0
            unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]'
            ;;
        valid-int-boundary) params[queue.timeoutshutdown]=2147483647 ;;
        valid-global-legacy)
            params[queue.scope]=global
            unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]'
            preamble='$ActionResumeRetryCount 2147483648'
            statement='*.* /dev/null;local_test'
            ;;
        valid-imtcp) module_name=imtcp; input_type=imtcp ;;
        invalid-input) module_name=imptcp; input_type=imptcp ;;
        valid-impstats|invalid-impstats)
            module_name=impstats
            local syslog=off
            [ "$scenario" = invalid-impstats ] && syslog=on
            module_rs="log.syslog=\"$syslog\" log.file=\"/dev/null\""
            module_yaml="    log.syslog: \"$syslog\"
    log.file: \"/dev/null\""
            ;;
        custom-parser)
            preamble='parser(name="custom.local_test" type="pmrfc3164" detect.headerless="on" headerless.ruleset="main")'
            ;;
        inherited-action) preamble='$ActionResumeRetryCount 1'; statement='*.* /dev/null;local_test' ;;
        inherited-wrapped-action) preamble='$ActionResumeRetryCount 2147483648' ;;
        inherited-wrapped-then-reset) preamble=$'$ActionResumeRetryCount 2147483648\n$ActionResumeRetryCount 0' ;;
        inherited-file) preamble='$OMFileAsyncWriting on'; statement='*.* /dev/null;local_test' ;;
        inherited-module)
            preamble='module(load="builtin:omfile" fileOwnerNum="4294967296")'
            ;;
        global-graph-escape) extra_ruleset='set $/shared = 1;' ;;
        duplicate-main) duplicate_main=1 ;;
        developer-bypass) global_extra='internal.developeronly.options="1"'; params[queue.scope]=locla ;;
        shutdown-double-size) global_extra='shutdown.queue.doubleSize="on"' ;;
        wrapped-minimum) params[queue.mindequeuebatchsize]=4294967296 ;;
        wrapped-sampling) params[queue.samplinginterval]=4294967296 ;;
        wrapped-severity) params[queue.discardseverity]=4294967304 ;;
        wrapped-timeout) params[queue.timeoutshutdown]=4294967296 ;;
        overflow-timeout) params[queue.timeoutshutdown]=2147483648 ;;
        negative-timeout) params[queue.timeoutshutdown]=-1 ;;
        wrapped-action) action_extra='action.execOnlyEveryNthTime="4294967296"' ;;
        wrapped-retry) action_extra='action.resumeRetryCount="4294967296"' ;;
        wrapped-file) action_extra='zipLevel="4294967296"' ;;
        wrapped-close) action_extra='closeTimeout="65535"' ;;
        valid-global) params[queue.scope]=global; unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]' ;;
        valid-disabled)
            params[queue.scope]=global
            unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]'
            statement='action(config.enabled="off" type="not_a_module" queue.scope="locla")'
            ;;
        deleted-local)
            params[queue.scope]=global
            unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]'
            # An invalid local request must survive dead-branch optimization.
            statement='if 0 then action(type="omfile" file="/dev/null" template="local_test" queue.scope="local" queue.type="FixedArray" queue.local.frontendSize="0" queue.local.maxFrontends="2")'
            ;;
        dropped-local)
            params[queue.scope]=global
            unset 'params[queue.local.frontendsize]' 'params[queue.local.maxfrontends]'
            statement='action(type="not_a_module" queue.scope="local")'
            ;;
        bad-scope) params[queue.scope]=locla ;;
        missing-bound) unset 'params[queue.local.maxfrontends]' ;;
        negative-bound) params[queue.local.frontendsize]=-1 ;;
        oversized-bound) params[queue.local.frontendsize]=4294967396 ;;
        disk) params[queue.type]=Disk ;;
        direct) params[queue.type]=Direct ;;
        minimum) params[queue.mindequeuebatchsize]=10 ;;
        valid-min-timeout) params[queue.mindequeuebatchsize]=10; params[queue.mindequeuebatchsize.timeout]=20 ;;
        valid-linkedlist) params[queue.type]=LinkedList ;;
        valid-slowdown) params[queue.dequeueslowdown]=1 ;;
        valid-schedule) params[queue.dequeuetimebegin]=1; params[queue.dequeuetimeend]=23 ;;
        valid-discard-mark) params[queue.discardseverity]=5; params[queue.discardmark]=3 ;;
        valid-severity-name) params[queue.discardseverity]=warning ;;
        valid-schedule-disabled) params[queue.dequeuetimeend]=25 ;;
        severity-name-invalid) params[queue.discardseverity]=notaseverity ;;
        minimum-timeout-overflow) params[queue.mindequeuebatchsize.timeout]=4294967296 ;;

        negative-sampling) params[queue.samplinginterval]=-1 ;;
        negative-minimum) params[queue.mindequeuebatchsize]=-1 ;;
        minimum-above-batch) params[queue.mindequeuebatchsize]=129; params[queue.dequeuebatchsize]=128 ;;
        negative-min-timeout) params[queue.mindequeuebatchsize.timeout]=-1 ;;
        negative-slowdown) params[queue.dequeueslowdown]=-1 ;;
        schedule-start-range) params[queue.dequeuetimebegin]=24 ;;
        schedule-end-range) params[queue.dequeuetimeend]=24 ;;
        schedule-overflow) params[queue.dequeuetimebegin]=4294967296 ;;
        negative-severity) params[queue.discardseverity]=-1 ;;
        severity-range) params[queue.discardseverity]=9 ;;
        discard-above-reservation) params[queue.discardmark]=1401 ;;
        allocation-overflow) params[queue.local.frontendsize]=2147483647; params[queue.local.maxfrontends]=2147483647 ;;


        sampling) params[queue.samplinginterval]=2 ;;
        discard) params[queue.discardseverity]=5 ;;
        disk-option) params[queue.syncqueuefiles]=on ;;
        valid-da-classic|valid-da-segmented|valid-da-save-disabled)
            params[queue.filename]="${RSYSLOG_DYNNAME}.queue"
            params[queue.saveonshutdown]=on
            params[queue.diskqueuetype]=disk
            [ "$scenario" != valid-da-segmented ] || params[queue.diskqueuetype]=segmentedDisk
            [ "$scenario" != valid-da-save-disabled ] || params[queue.saveonshutdown]=off
            ;;
        valid-memory-save) params[queue.saveonshutdown]=on ;;

        asyncfile) action_extra='asyncWriting="on"' ;;
        syncfile) action_extra='sync="on"' ;;
        unflushed-file) action_extra='flushOnTXEnd="off"' ;;
        queued-action) action_extra='queue.type="FixedArray"' ;;
        unsafe-function) statement='set $.v = getenv("HOME");' ;;
        unsafe-destination) statement='set $.v = parse_json("{}", "\$/shared");' ;;
        malformed-destination) statement='set $.v = parse_json("{}", "\$!a!!b");' ;;
        valid-json) statement='set $.v = parse_json("{\"n\":1}", "\$!bench");' ;;
        shared-variable) statement='set $/shared = 1;' ;;
        call) statement='call main' ;;
        indirect) statement='call_indirect "main";' ;;
        *) error_exit 1 ;;
    esac
    if [ "${LOCAL_QUEUE_CONFIG_YAML:-0}" -eq 1 ]; then
        conf="${RSYSLOG_DYNNAME}.${scenario}.yaml"
        {
            printf 'global:\n  abortOnUncleanConfig: "off"\n'
            case "$scenario" in
                developer-bypass) printf '  internal.developeronly.options: "1"\n' ;;
                shutdown-double-size) printf '  shutdown.queue.doubleSize: "on"\n' ;;
            esac
            if [ -n "$module_name" ]; then
                printf 'modules:\n  - load: "../plugins/%s/.libs/%s"\n' "$module_name" "$module_name"
                [ -z "$module_yaml" ] || printf '%s\n' "$module_yaml"
            fi
            if [ -n "$input_type" ]; then
                printf 'inputs:\n  - type: %s\n    address: "127.0.0.1"\n    port: "0"\n    ruleset: main\n' "$input_type"
            fi
            printf 'mainqueue:\n'
            for key in "${!params[@]}"; do
                printf '  %s: "%s"\n' "$key" "${params[$key]}"
            done
            cat <<'YAML'
templates:
  - name: local_test
    type: string
    string: "%msg%\n"
rulesets:
  - name: main
    script: |
YAML
            printf '      %s\n' "$statement"
            printf '      action(type="omfile" file="/dev/null" template="local_test" %s)\n' "$action_extra"
            [ -z "$extra_ruleset" ] || printf '  - name: unreachable_global\n    script: |\n      %s\n' "$extra_ruleset"
        } > "$conf"
    else
        conf="${RSYSLOG_DYNNAME}.${scenario}.conf"
        {
            printf 'global(abortOnUncleanConfig="off" %s)\n' "$global_extra"
            [ -z "$module_name" ] || printf 'module(load="../plugins/%s/.libs/%s" %s)\n' "$module_name" "$module_name" "$module_rs"
            [ -z "$input_type" ] || printf 'input(type="%s" address="127.0.0.1" port="0" ruleset="main")\n' "$input_type"
            printf 'main_queue(\n'
            for key in "${!params[@]}"; do
                printf '  %s="%s"\n' "$key" "${params[$key]}"
            done
            printf ')\ntemplate(name="local_test" type="string" string="%%msg%%\\n")\n'
            printf 'ruleset(name="main") {\n%s\n' "$statement"
            printf 'action(type="omfile" file="/dev/null" template="local_test" %s)\n}\n' "$action_extra"
            [ -z "$extra_ruleset" ] || printf 'ruleset(name="unreachable_global") { %s }\n' "$extra_ruleset"
        } > "$conf"
    fi
    # Legacy directives enter through a RainerScript include wrapper for both
    # frontends. Local intent must survive earlier fragments and later objects.
    if [ -n "$preamble" ] || [ "$duplicate_main" -eq 1 ]; then
        local original="$conf"
        conf="${RSYSLOG_DYNNAME}.${scenario}.wrapper.conf"
        {
            printf '%s\n' "$preamble"
            printf 'include(file="%s")\n' "$original"
            [ "$duplicate_main" -eq 0 ] || printf 'main_queue(queue.scope="global")\n'
        } > "$conf"
    fi
}

positive=(valid-severity-name valid-schedule-disabled minimum sampling discard valid-min-timeout valid-linkedlist valid-slowdown valid-schedule valid-discard-mark valid-da-classic valid-da-segmented valid-da-save-disabled valid-memory-save disk-option valid-helper-default valid-helper-zero valid-helper-cap valid valid-json valid-global valid-disabled valid-int-boundary valid-global-legacy queued-action inherited-action)
negative=(severity-name-invalid minimum-timeout-overflow negative-sampling negative-minimum minimum-above-batch negative-min-timeout negative-slowdown
    schedule-start-range schedule-end-range schedule-overflow negative-severity severity-range
    discard-above-reservation allocation-overflow helper-negative helper-above-cap helper-overflow helper-global bad-scope missing-bound negative-bound oversized-bound disk direct
    deleted-local dropped-local asyncfile syncfile unflushed-file
    unsafe-function unsafe-destination malformed-destination shared-variable call indirect
    wrapped-minimum wrapped-sampling wrapped-severity wrapped-timeout overflow-timeout negative-timeout
    wrapped-action wrapped-retry wrapped-file wrapped-close custom-parser
    inherited-wrapped-action inherited-wrapped-then-reset inherited-file inherited-module global-graph-escape
    duplicate-main developer-bypass shutdown-double-size)
for module_name in imtcp imptcp impstats; do
    if [ ! -f "../plugins/$module_name/.libs/$module_name.so" ]; then
        echo "SKIP optional $module_name qualification cases: module not built"
        continue
    fi
    case "$module_name" in
        imtcp) positive+=(valid-imtcp) ;;
        imptcp) negative+=(invalid-input) ;;
        impstats) positive+=(valid-impstats); negative+=(invalid-impstats) ;;
    esac
done

for scenario in "${positive[@]}"; do
    write_config "$scenario"
    if ! "$timeout_cmd" -k 2 20 ../tools/rsyslogd -N1 -f "$conf" -M../runtime/.libs:../.libs \
        > "${RSYSLOG_DYNNAME}.${scenario}.log" 2>&1; then
        cat "${RSYSLOG_DYNNAME}.${scenario}.log"
        error_exit 1
    fi
done

for scenario in "${negative[@]}"; do
    write_config "$scenario"
    for mode in -N1 -N3 -n; do
        log="${RSYSLOG_DYNNAME}.${scenario}.${mode}.log"
        "$timeout_cmd" -k 2 20 ../tools/rsyslogd "$mode" -i "${RSYSLOG_DYNNAME}.pid" \
            -f "$conf" -M../runtime/.libs:../.libs > "$log" 2>&1
        result=$?
        if [ "$result" -eq 0 ] || [ "$result" -eq 124 ] || [ "$result" -eq 137 ] ||
            ! grep -F -- '-2466' "$log" > /dev/null; then
            echo "FAIL: $scenario ($mode) did not fail with the local-config error"
            cat "$log"
            error_exit 1
        fi
        # These configurations must reach their intended qualification branch;
        # an unrelated syntax or module-loading error would be a false positive.
        case "$scenario" in
            invalid-input|invalid-impstats)
                grep -F "input module" "$log" > /dev/null || { cat "$log"; error_exit 1; }
                ;;
            custom-parser)
                grep -F "explicit parser instances" "$log" > /dev/null || { cat "$log"; error_exit 1; }
                ;;
        esac
        case "$scenario" in
            invalid-input|invalid-impstats|custom-parser|inherited-*|wrapped-*)
                if grep -E 'syntax error|could not load module|parameter .* not known' "$log" > /dev/null; then
                    cat "$log"
                    error_exit 1
                fi
                ;;
        esac
    done
done
exit_test
