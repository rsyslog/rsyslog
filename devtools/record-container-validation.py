#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Record reviewed local validation evidence; an agent guard, not an attestation.

Evidence is a local operator-supplied JSON manifest. Never execute its contents.
Validate the source tree, pinned environment and test logs before advancing the
existing commit marker. The companion record preserves a non-clean broad result.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


RELEVANT = re.compile(
    r"\.(c|h|sh|py)$|Makefile\.am|configure\.ac|Dockerfile|MODULE_METADATA\.yaml|^tests/"
    r"|^grammar/(lexer\.l|grammar\.y)$|^\.github/workflows/run_checks\.yml$"
)


def git(tree, *args):
    return subprocess.check_output(['git', '-C', str(tree), *args], stderr=subprocess.PIPE)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def inventory(tree, commit):
    result = {}
    for entry in git(tree, 'ls-tree', '-rz', commit).split(b'\0'):
        if not entry:
            continue
        meta, name = entry.split(b'\t', 1)
        path = os.fsdecode(name)
        mode, kind, oid = meta.decode().split()
        if RELEVANT.search(path):
            require(kind == 'blob' and mode in ('100644', '100755'), 'unsupported source entry: ' + path)
            result[path] = (mode == '100755', git(tree, 'cat-file', 'blob', oid))
    require(result, 'source inventory is empty')
    return result


def check_tree(tree, expected):
    tracked = {os.fsdecode(p) for p in git(tree, 'ls-files', '-z').split(b'\0') if p}
    extra_paths = git(tree, 'ls-files', '--others', '--exclude-standard', '-z')
    untracked = {os.fsdecode(p) for p in extra_paths.split(b'\0') if p}
    # A disposable checkout may contain expected files applied without git add.
    # They are content/mode checked below; any additional relevant file fails.
    require(not any(RELEVANT.search(p) for p in untracked - expected.keys()),
            'extra untracked source in ' + str(tree))
    for path in tracked - expected.keys():
        require(not RELEVANT.search(path) or not (tree / path).exists(), 'extra tracked source: ' + path)
    digest = hashlib.sha256()
    for path, (executable, content) in sorted(expected.items()):
        current = tree / path
        require(not current.is_symlink() and current.is_file(), 'missing/nonregular source: ' + str(current))
        require(current.read_bytes() == content, 'source content mismatch: ' + str(current))
        require(bool(current.stat().st_mode & 0o111) == executable, 'source mode mismatch: ' + str(current))
        digest.update(path.encode() + b'\0' + bytes([executable]) + hashlib.sha256(content).digest())
    return digest.hexdigest()


def log_text(base, record, hashes):
    relative = Path(record['log'])
    require(not relative.is_absolute(), 'log paths must be relative to the evidence manifest')
    path = (base / relative).resolve()
    require(base == path.parent or base in path.parents, 'log path escapes the evidence directory')
    data = path.read_bytes()
    hashes[str(path)] = hashlib.sha256(data).hexdigest()
    return data.decode('utf-8', errors='replace')


def atomic_write(path, text):
    fd, name = tempfile.mkstemp(prefix=path.name + '.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(text)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def record(args):
    evidence_path = args.evidence.resolve()
    evidence = json.loads(evidence_path.read_text())
    require(isinstance(evidence, dict), 'evidence must be a JSON object')
    require(evidence.get('schema_version') == 1, 'unsupported evidence schema')
    target = Path(git(args.target, 'rev-parse', '--show-toplevel').decode().strip())
    validation = Path(evidence['validation_tree']).resolve()
    commit = git(target, 'rev-parse', '--verify', evidence['source_commit'] + '^{commit}').decode().strip()
    subprocess.run(['git', '-C', str(target), 'merge-base', '--is-ancestor', commit, 'HEAD'], check=True)
    expected = inventory(target, commit)
    # A documentation-only tail is allowed; a tracked runtime addition is not.
    require(inventory(target, 'HEAD') == expected, 'target HEAD differs in source/build/test content')
    source_digest = check_tree(target, expected)
    require(check_tree(validation, expected) == source_digest, 'validation tree differs')
    image = evidence['image']
    require(re.fullmatch(r'.+@sha256:[0-9a-f]{64}', image) is not None, 'image must include pinned sha256 digest')
    hashes = {}
    broad = evidence['broad']
    require(broad['source_commit'] == commit and broad['image'] == image, 'broad source/image mismatch')
    require(bool(broad.get('command')), 'broad command is required')
    text = log_text(evidence_path.parent, broad, hashes)
    counts = {}
    for key in ('TOTAL', 'PASS', 'SKIP', 'XFAIL', 'FAIL', 'XPASS', 'ERROR'):
        values = re.findall(r'^#\s*' + key + r':\s*(\d+)\s*$', text, re.M)
        require(len(values) == 1, 'use one complete Automake test-suite.log: missing/duplicate ' + key)
        counts[key] = int(values[0])
    require(counts['TOTAL'] > 0 and counts['PASS'] > 0, 'no passing tests in broad run')
    require(sum(v for k, v in counts.items() if k != 'TOTAL') == counts['TOTAL'], 'inconsistent broad counts')
    require(counts['ERROR'] == 0 and counts['XPASS'] == 0, 'errors/unexpected passes cannot be reconciled')
    failed = re.findall(r'^FAIL:\s+(\S+)\s*$', text, re.M)
    require(len(failed) == counts['FAIL'] and len(set(failed)) == len(failed), 'failed-test inventory mismatch')
    reruns = evidence.get('reruns', [])
    require(len(reruns) == len(failed) and {r['test'] for r in reruns} == set(failed),
            'reruns must cover exactly failures')
    for rerun in reruns:
        require(rerun['source_commit'] == commit and rerun['image'] == image, 'rerun source/image mismatch')
        require(rerun['exit_code'] == 0 and bool(rerun.get('command')), 'rerun unsuccessful or command missing')
        log = log_text(evidence_path.parent, rerun, hashes)
        test = re.escape(rerun['test'])
        script = test if rerun['test'].endswith('.sh') else test + r'(?:\.sh)?'
        # Automake test-suite.log headings omit the .sh LOG_COMPILER suffix.
        # Permit exactly that suffix for direct diag.sh reruns; no substring match.
        success = re.search(r'^PASS:\s+' + test + r'\s*$', log, re.M)
        success = success or re.search(r'\bTest (?:\./)?' + script + r' SUCCESSFUL \(took ', log)
        require(success and not re.search(r'^(?:FAIL|ERROR):', log, re.M), 'rerun log lacks clean exact-test success')
    require(broad['exit_code'] == (2 if failed else 0), 'broad exit code disagrees with result')
    if failed:
        require(bool(args.accept_flakes and args.accept_flakes.strip()), 'explicit --accept-flakes rationale required')
    require(isinstance(evidence.get('coverage_limits'), list), 'coverage_limits must explicitly list omissions (or [])')
    receipt = dict(schema_version=1, source_commit=commit, source_digest=source_digest,
                   target_head=git(target, 'rev-parse', 'HEAD').decode().strip(), image=image,
                   outcome='flake_reconciled' if failed else 'pass', broad_clean=not failed,
                   counts=counts, failed_tests=failed, acceptance=args.accept_flakes,
                   coverage_limits=evidence['coverage_limits'], evidence=evidence, log_sha256=hashes)
    directory = target / '.codex'
    require(not directory.is_symlink(), '.codex must not be a symlink')
    directory.mkdir(exist_ok=True)
    # Write evidence first; a failure must not advance the marker.
    atomic_write(directory / 'container_validation.json', json.dumps(receipt, indent=2) + '\n')
    atomic_write(directory / 'container_validated.marker', commit + '\n')
    print('Recorded ' + receipt['outcome'] + ' for ' + commit + ' in ' + str(target))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--accept-flakes',
                        help='explicit maintainer acceptance rationale; not an automatic flake verdict')
    args = parser.parse_args()
    try:
        record(args)
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Validation not recorded: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
