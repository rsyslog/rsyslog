#!/bin/sh
# Verify the recorder never advances a marker for stale source, incomplete or
# mismatched reruns, unaccepted flakes, or invalid broad evidence. Synthetic git
# trees and exact result logs provide deterministic oracles; no daemon/network.
# The original broad failure remains in the audit even after accepted reruns.
set -eu
command -v python3 >/dev/null 2>&1 || exit 77
command -v git >/dev/null 2>&1 || exit 77
test_srcdir="${srcdir:-$(dirname "$0")}"
python3 - "$test_srcdir/../devtools/record-container-validation.py" <<'PY'
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile

recorder = Path(sys.argv[1]).resolve()

def git(tree, *args):
    return subprocess.check_output(['git', '-C', str(tree), *args], stderr=subprocess.DEVNULL).decode().strip()

with tempfile.TemporaryDirectory(prefix='rsyslog-validation-recorder-') as temporary:
    root = Path(temporary)
    target = root / 'target'
    validated = root / 'validated'
    target.mkdir()
    git(target, 'init', '-q')
    git(target, 'config', 'user.email', 'test@example.invalid')
    git(target, 'config', 'user.name', 'Test')
    git(target, 'config', 'commit.gpgsign', 'false')
    git(target, 'config', 'core.hooksPath', '/dev/null')
    (target / 'runtime').mkdir()
    (target / 'runtime/a.c').write_text('original\n')
    git(target, 'add', '.')
    git(target, 'commit', '-qm', 'source')
    commit = git(target, 'rev-parse', 'HEAD')
    subprocess.run(['git', 'clone', '-q', str(target), str(validated)], check=True)
    (target / 'README.md').write_text('docs only\n')
    git(target, 'add', '.')
    git(target, 'commit', '-qm', 'docs')
    image = 'example.invalid/dev@sha256:' + 'a' * 64
    broad = root / 'test-suite.log'
    def summary(fail=1, error=0):
        broad.write_text(f'# TOTAL: 3\n# PASS: {3-fail-error}\n# SKIP: 0\n# XFAIL: 0\n'
                         f'# FAIL: {fail}\n# XPASS: 0\n# ERROR: {error}\n' + ('FAIL: flaky\n' if fail else ''))
    summary()
    (root / 'retry.log').write_text('00:00:00[2] Test ./flaky.sh SUCCESSFUL (took 2 seconds)\n')
    identity = dict(source_commit=commit, image=image)
    evidence = dict(schema_version=1, source_commit=commit, validation_tree=str(validated), image=image,
                    coverage_limits=['Elasticsearch not configured'],
                    broad=dict(identity, exit_code=2, command='devtools/run-ci.sh', log='test-suite.log'),
                    reruns=[dict(identity, test='flaky', exit_code=0, command='./flaky.sh', log='retry.log')])
    manifest = root / 'evidence.json'
    marker = target / '.codex/container_validated.marker'
    def run(data, expected, accepted=True):
        manifest.write_text(json.dumps(data))
        prior = marker.read_bytes() if marker.exists() else None
        args = [sys.executable, str(recorder), '--target', str(target), '--evidence', str(manifest)]
        if accepted:
            args += ['--accept-flakes', 'Maintainer accepts the isolated successful retry for this campaign']
        result = subprocess.run(args, text=True, capture_output=True)
        assert (result.returncode == 0) == expected, result.stdout + result.stderr
        if not expected:
            assert (marker.read_bytes() if marker.exists() else None) == prior
    run(evidence, False, accepted=False)
    run(evidence, True)
    assert marker.read_text().strip() == commit
    audit = json.loads((target / '.codex/container_validation.json').read_text())
    assert audit['outcome'] == 'flake_reconciled' and audit['broad_clean'] is False
    assert audit['failed_tests'] == ['flaky'] and len(audit['log_sha256']) == 2
    for key, value in [('reruns', []), ('image', 'unversioned:latest')]:
        bad = copy.deepcopy(evidence); bad[key] = value
        run(bad, False)
    for key, value in [('image', image[:-1] + 'b'), ('source_commit', '0'*40),
                       ('exit_code', 1), ('test', 'different')]:
        bad = copy.deepcopy(evidence); bad['reruns'][0][key] = value
        run(bad, False)
    (root / 'retry.log').write_text('PASS: different\n')
    run(evidence, False)
    (root / 'retry.log').write_text('PASS: flaky\n')
    for tree in (target, validated):
        (tree / 'runtime/a.c').write_text('modified\n')
        run(evidence, False)
        (tree / 'runtime/a.c').write_text('original\n')
    (target / 'new.c').write_text('untracked\n')
    run(evidence, False)
    (target / 'new.c').unlink()
    summary(fail=0, error=1)
    run(evidence, False)
    summary(fail=0)
    clean = copy.deepcopy(evidence); clean['reruns'] = []; clean['broad']['exit_code'] = 0
    run(clean, True, accepted=False)
    audit = json.loads((target / '.codex/container_validation.json').read_text())
    assert audit['outcome'] == 'pass' and audit['broad_clean'] is True
    (target / 'runtime/new.c').write_text('added\n')
    git(target, 'add', 'runtime/new.c'); git(target, 'commit', '-qm', 'new source')
    run(clean, False)
print('record-container-validation: PASS')
PY
