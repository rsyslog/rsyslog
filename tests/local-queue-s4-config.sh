#!/bin/bash
# Validate the same static graph through native RainerScript and YAML queue
# objects. An acyclic graph must pass; a cycle and a dynamic call must fail
# before activation. -N1 status plus the dedicated local-config diagnostic is
# the oracle; timeout is only a hang guard. No listener or output is started.
# This file is part of the rsyslog project, released under ASL 2.0.
. ${srcdir:=.}/diag.sh init
timeout_cmd=${timeout_cmd:-timeout}
command -v "$timeout_cmd" >/dev/null 2>&1 || timeout_cmd=gtimeout
command -v "$timeout_cmd" >/dev/null 2>&1 || {
    echo 'Testbench requires unavailable command: timeout or gtimeout'
    exit 77
}
if [ "${LOCAL_QUEUE_S4_YAML:-0}" -eq 1 ]; then require_yaml_support; fi
for scenario in valid cycle indirect; do
    CONF="$PWD/$RSYSLOG_DYNNAME.$scenario.conf"
    if [ "${LOCAL_QUEUE_S4_YAML:-0}" -eq 1 ]; then CONF="${CONF%.conf}.yaml"; fi
    python3 - "$CONF" "$scenario" "${LOCAL_QUEUE_S4_YAML:-0}" <<'PY'
import json
import sys
from pathlib import Path
filename, scenario, yaml = sys.argv[1:]
queue = {'queue.type': 'FixedArray', 'queue.scope': 'local', 'queue.size': '1000',
         'queue.local.frontendSize': '100', 'queue.local.maxFrontends': '4'}
action = {'type': 'omfile', 'file': '/dev/null', 'template': 'msg', 'action.copyMsg': 'on', **queue}
leaf = 'call root' if scenario == 'cycle' else ''
root = 'call_indirect "leaf";' if scenario == 'indirect' else 'call leaf'
if yaml == '1':
    # JSON scalars are also YAML scalars; emit native objects without requiring
    # PyYAML in test environments. Script bodies exercise the shared AST.
    text = 'templates:\n  - name: msg\n    type: string\n    string: "%msg%\\n"\nrulesets:\n'
    for name, script in [('leaf', leaf), ('root', root)]:
        text += f'  - name: {name}\n'
        text += ''.join(f'    {key}: {json.dumps(value)}\n' for key, value in queue.items())
        if script:
            text += f'    script: {json.dumps(script)}\n'
        else:
            text += '    statements:\n      - ' + '\n        '.join(
                f'{key}: {json.dumps(value)}' for key, value in action.items()) + '\n'
else:
    attrs = ' '.join(f'{key}={json.dumps(value)}' for key, value in queue.items())
    sink = 'action(' + ' '.join(f'{key}={json.dumps(value)}' for key, value in action.items()) + ')'
    text = 'template(name="msg" type="string" string="%msg%\\n")\n'
    text += f'ruleset(name="leaf" {attrs}) {{ {leaf or sink} }}\n'
    text += f'ruleset(name="root" {attrs}) {{ {root} }}\n'
Path(filename).write_text(text)
PY
    LOG="$PWD/$RSYSLOG_DYNNAME.$scenario.log"
    $timeout_cmd -k 2 20 ../tools/rsyslogd -N1 -f "$CONF" -M../runtime/.libs:../.libs >"$LOG" 2>&1
    result=$?
    if [ "$scenario" = valid ]; then
        if [ "$result" -ne 0 ]; then cat "$LOG"; error_exit 1; fi
    elif [ "$result" -eq 0 ] || [ "$result" -eq 124 ] || [ "$result" -eq 137 ] ||
        ! grep -F -- '-2466' "$LOG" >/dev/null; then
        cat "$LOG"
        error_exit 1
    fi
    if grep -E 'syntax error|parameter .* not known' "$LOG" >/dev/null; then
        cat "$LOG"
        error_exit 1
    fi
done
exit_test
