#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A module template supplies the omitted modern action template; an explicit
# action template overrides it. Legacy selectors retain the module default.
# Without a module default, the implicit action retains RSYSLOG_FileFormat.
# The YAML companion exercises the same modern actions. FIFO readers consume
# exactly one injected message each; bounded line waits and exact payloads
# prove delivery and formatting before shutdown. Tracked readers are reaped or
# killed on every exit, so a failed formatting assertion cannot leak helpers.
. ${srcdir:=.}/diag.sh init

reader_pids=()
cleanup_readers() {
	local pid
	for pid in "${reader_pids[@]}"; do
		kill "$pid" 2>/dev/null || :
		wait "$pid" 2>/dev/null || :
	done
}
trap cleanup_readers EXIT

start_reader() {
	local name="$1"
	mkfifo "${RSYSLOG_DYNNAME}.${name}.fifo" || error_exit 1 "mkfifo failed"
	head -n 1 "${RSYSLOG_DYNNAME}.${name}.fifo" > "${RSYSLOG_DYNNAME}.${name}.out" &
	reader_pids+=("$!")
}
start_reader implicit
start_reader explicit

generate_conf
if [ "${OMPIPE_TEMPLATE_UNSET:-}" = YES ]; then
	add_conf '
if ($msg contains "msgnum:") then {
    action(type="ompipe" pipe="'${RSYSLOG_DYNNAME}'.implicit.fifo")
    action(type="ompipe" pipe="'${RSYSLOG_DYNNAME}'.explicit.fifo" template="RSYSLOG_FileFormat")
}
'
elif [ "${OMPIPE_TEMPLATE_YAML:-}" = YES ]; then
	cat > "${RSYSLOG_DYNNAME}.yaml" <<YAML_EOF
version: 2
modules:
  - load: builtin:ompipe
    template: modulefmt
templates:
  - name: modulefmt
    type: string
    string: "module:%msg:F,58:2%\\n"
  - name: actionfmt
    type: string
    string: "action:%msg:F,58:2%\\n"
rulesets:
  - name: pipeactions
    filter: ':msg, contains, "msgnum:"'
    actions:
      - type: ompipe
        pipe: "${RSYSLOG_DYNNAME}.implicit.fifo"
      - type: ompipe
        pipe: "${RSYSLOG_DYNNAME}.explicit.fifo"
        template: actionfmt
YAML_EOF
	add_conf 'include(file="'${RSYSLOG_DYNNAME}'.yaml")
call pipeactions'
else
	start_reader legacy
	add_conf '
module(load="builtin:ompipe" template="modulefmt")
template(name="modulefmt" type="string" string="module:%msg:F,58:2%\n")
template(name="actionfmt" type="string" string="action:%msg:F,58:2%\n")
if ($msg contains "msgnum:") then {
    action(type="ompipe" pipe="'${RSYSLOG_DYNNAME}'.implicit.fifo")
    action(type="ompipe" pipe="'${RSYSLOG_DYNNAME}'.explicit.fifo" template="actionfmt")
}
:msg, contains, "msgnum:" |'${RSYSLOG_DYNNAME}'.legacy.fifo
'
fi
startup
injectmsg 0 1
for name in implicit explicit; do
	wait_file_lines "${RSYSLOG_DYNNAME}.${name}.out" 1
done
if [ "${OMPIPE_TEMPLATE_YAML:-}" != YES ] && [ "${OMPIPE_TEMPLATE_UNSET:-}" != YES ]; then
	wait_file_lines "${RSYSLOG_DYNNAME}.legacy.out" 1
fi
shutdown_when_empty
wait_shutdown
for pid in "${reader_pids[@]}"; do
	wait "$pid" || error_exit 1 "FIFO reader failed"
done
reader_pids=()

if [ "${OMPIPE_TEMPLATE_UNSET:-}" = YES ]; then
	cmp_exact_file "${RSYSLOG_DYNNAME}.explicit.out" "${RSYSLOG_DYNNAME}.implicit.out"
	printf 'unset module default equals RSYSLOG_FileFormat: PASS\n'
	exit_test
fi

# Assert controls first so the base failure specifically identifies the
# modern omitted-template path, rather than a broken reader or module setup.
export EXPECTED='action:00000000'
cmp_exact "${RSYSLOG_DYNNAME}.explicit.out"
printf 'explicit action override control: PASS\n'
export EXPECTED='module:00000000'
if [ "${OMPIPE_TEMPLATE_YAML:-}" != YES ] && [ "${OMPIPE_TEMPLATE_UNSET:-}" != YES ]; then
	cmp_exact "${RSYSLOG_DYNNAME}.legacy.out"
	printf 'legacy module-default control: PASS\n'
fi
cmp_exact "${RSYSLOG_DYNNAME}.implicit.out"
exit_test
