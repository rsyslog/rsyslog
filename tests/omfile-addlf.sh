. ${srcdir:=.}/diag.sh init

wait_for_nonempty_file() {
    file="$1"
    timeout=${2:-60}
    waited=0
    while [ $waited -lt $timeout ]; do
        if [ -s "$file" ]; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    error_exit 1 "timeout waiting for $file to become non-empty"
}

generate_conf
add_conf '
module(load="builtin:omfile" addLF="on")
template(name="rawfmt" type="string" string="%msg%")
if $msg contains "use_module_default" then {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.module" template="rawfmt")
    stop
}
if $msg contains "explicit_flag" then {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.explicit" template="rawfmt" addLF="on")
    stop
}
if $msg contains "force_off" then {
    action(type="omfile" file="'$RSYSLOG_OUT_LOG'.override" template="rawfmt" addLF="off")
    stop
}
'

startup
injectmsg_literal '<165>1 2024-01-01T00:00:00.000000Z host app - - - use_module_default'
injectmsg_literal '<165>1 2024-01-01T00:00:01.000000Z host app - - - explicit_flag'
injectmsg_literal '<165>1 2024-01-01T00:00:02.000000Z host app - - - force_off'
shutdown_when_empty
wait_shutdown

wait_file_lines "$RSYSLOG_OUT_LOG.module" 1
wait_file_lines "$RSYSLOG_OUT_LOG.explicit" 1
wait_for_nonempty_file "$RSYSLOG_OUT_LOG.override"

printf 'use_module_default\n' > "$RSYSLOG_OUT_LOG.module.expected"
printf 'explicit_flag\n' > "$RSYSLOG_OUT_LOG.explicit.expected"
printf 'force_off' > "$RSYSLOG_OUT_LOG.override.expected"

cmp "$RSYSLOG_OUT_LOG.module.expected" "$RSYSLOG_OUT_LOG.module" || error_exit 1 "module default addLF mismatch"
cmp "$RSYSLOG_OUT_LOG.explicit.expected" "$RSYSLOG_OUT_LOG.explicit" || error_exit 1 "explicit addLF mismatch"
cmp "$RSYSLOG_OUT_LOG.override.expected" "$RSYSLOG_OUT_LOG.override" || error_exit 1 "addLF override failed"

exit_test
