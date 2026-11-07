#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Exercise the real local validation dispatcher in a temporary Git tree.
# Stub container lanes write a known Automake log; analyzer distclean removes
# its original. Exact bytes, exit status, and missing-log diagnostics are the
# oracles. The developer-only dispatcher is absent from release tarballs, so
# that case skips while the source-tree test exercises the real script.
set -eu
command -v python3 >/dev/null 2>&1 || exit 77
command -v git >/dev/null 2>&1 || exit 77
test_srcdir="${srcdir:-$(dirname "$0")}"
test -f "$test_srcdir/../devtools/local-validation-plan.sh" || exit 77
python3 - "$test_srcdir/../devtools/local-validation-plan.sh" <<'PY'
from pathlib import Path
import os
import subprocess
import sys
import tempfile

planner = Path(sys.argv[1]).resolve()
log = b'# TOTAL: 1\n# PASS: 1\n# SKIP: 0\n# XFAIL: 0\n# FAIL: 0\n# XPASS: 0\n# ERROR: 0\nPASS: example\n'

with tempfile.TemporaryDirectory(prefix='rsyslog-validation-log-') as temporary:
    root = Path(temporary)
    repo = root / 'repo'
    bin_dir = root / 'bin'
    repo.mkdir()
    bin_dir.mkdir()
    def write(path, content, executable=False):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        if executable:
            path.chmod(0o755)
    write(repo / 'devtools/local-validation-plan.sh', planner.read_text(), True)
    write(repo / 'devtools/check-raw-atomics.py', '')
    write(repo / 'devtools/apply-service-relevance.sh', 'rsyslog_apply_default_pr_service_suppressions() { :; }\n')
    write(repo / 'devtools/devcontainer.sh', '''#!/bin/sh
case "$2" in
*run-ci.sh)
    if [ "${STUB_LOG_MODE:-present}" = present ]; then
        mkdir -p tests
        cp "$STUB_EXPECTED_LOG" tests/test-suite.log
    fi
    exit "${STUB_CI_EXIT:-0}" ;;
*run-static-analyzer.sh) exit 0 ;;
esac
exit 9
''', True)
    write(bin_dir / 'docker', '#!/bin/sh\nexit 0\n', True)
    write(bin_dir / 'make', '''#!/bin/sh
if [ "$1" = distclean ]; then
    if [ "${STUB_DISTCLEAN_KEEP_LOG:-0}" != 1 ]; then
        rm -f tests/test-suite.log
    fi
    exit 0
fi
exit 9
''', True)
    expected = root / 'expected.log'
    expected.write_bytes(log)
    subprocess.run(['git', 'init', '-q', str(repo)], check=True)
    for args in [('config', 'user.email', 'test@example.invalid'),
                 ('config', 'user.name', 'Test'), ('config', 'commit.gpgsign', 'false'),
                 ('config', 'core.hooksPath', '/dev/null'), ('add', '.'),
                 ('commit', '-qm', 'baseline')]:
        subprocess.run(['git', '-C', str(repo), *args], check=True)
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    write(repo / 'runtime/change.c', 'int changed;\n')
    env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ['PATH'],
               STUB_EXPECTED_LOG=str(expected), RSYSLOG_LOCAL_VALIDATION_BASE=commit)
    def run(extra):
        return subprocess.run([str(repo / 'devtools/local-validation-plan.sh'), '--run'],
                              cwd=repo, env=dict(env, **extra), text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    result = run({})
    assert result.returncode == 0, result.stdout + result.stderr
    assert not (repo / 'tests/test-suite.log').exists(), 'analyzer did not remove original log'
    archived = list((repo / '.codex/local-validation-logs').glob('run-*/test-suite.log'))
    assert len(archived) == 1, archived
    assert archived[0].read_bytes() == log, 'archived log differs from original'
    metadata = (archived[0].parent / 'run.txt').read_text()
    assert f'source_commit={commit}\n' in metadata, metadata
    assert 'source_dirty=yes\n' in metadata, metadata
    assert 'run_ci_exit_code=0\n' in metadata, metadata
    assert 'configured_image_reference=rsyslog/rsyslog_dev_base_ubuntu:26.04\n' in metadata, metadata
    assert str(archived[0]) in result.stdout, result.stdout
    before = set(archived)
    result = run({'STUB_CI_EXIT': '7'})
    assert result.returncode == 7, result.stdout + result.stderr
    archived = list((repo / '.codex/local-validation-logs').glob('run-*/test-suite.log'))
    added = set(archived) - before
    assert len(added) == 1, archived
    failure_log = added.pop()
    assert failure_log.read_bytes() == log
    assert 'run_ci_exit_code=7\n' in (failure_log.parent / 'run.txt').read_text()
    write(repo / 'tests/test-suite.log', 'stale log from an earlier run\n')
    result = run({'STUB_LOG_MODE': 'absent', 'STUB_DISTCLEAN_KEEP_LOG': '1'})
    assert result.returncode != 0, result.stdout + result.stderr
    assert 'tests/test-suite.log' in result.stderr, result.stderr
    assert not (repo / 'tests/test-suite.log').exists(), 'stale log was accepted as fresh'
    assert len(list((repo / '.codex/local-validation-logs').glob('run-*/test-suite.log'))) == len(before) + 1
    result = run({'STUB_LOG_MODE': 'absent'})
    assert result.returncode != 0, result.stdout + result.stderr
    assert 'tests/test-suite.log' in result.stderr, result.stderr
    archived = list((repo / '.codex/local-validation-logs').glob('run-*/test-suite.log'))
    assert len(archived) == len(before) + 1, 'missing log created an archive'
    result = run({'STUB_LOG_MODE': 'absent', 'STUB_CI_EXIT': '7'})
    assert result.returncode == 7, result.stdout + result.stderr
    assert 'tests/test-suite.log' in result.stderr, result.stderr
    assert len(list((repo / '.codex/local-validation-logs').glob('run-*/test-suite.log'))) == len(before) + 1
print('local-validation-log-preservation: PASS')
PY
