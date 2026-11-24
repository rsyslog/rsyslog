#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify the daily-stable failure issue reporter against a fake GitHub API:
#
# - the likely cause is extracted from job logs (timestamps stripped, known
#   container noise dropped, unpackaged-file lists kept);
# - the cause fingerprint ignores versions, hashes, dates, architectures, and
#   job order, so the same failure on another night matches;
# - a first failure opens a labeled issue, the same cause again only updates
#   the "still failing" line of the latest report, and a new cause adds a
#   comment;
# - quoted log lines cannot close the code fence and are length-capped;
# - only issues and comments created by the workflow token are updated;
# - an unreadable job list still files a report.
#
# The oracle is the sequence of fake API calls and the resulting bodies.
# Needs node; the reporter is not distributed, so a tarball build skips it.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
reporter="$test_dir/../.github/scripts/report-daily-stable-failure.js"

command -v node >/dev/null 2>&1 || exit 77
[ -f "$reporter" ] || exit 77

node - "$reporter" <<'JS'
const assert = require('assert');
const report = require(process.argv[2]);

const unpackaged = (arch, day) => [
  `2026-10-${day}T10:14:43.4271807Z error: Installed (but unpackaged) file(s) found:`,
  `2026-10-${day}T10:14:43.4271807Z    /usr/lib/debug/usr/lib64/rsyslog/omkinesis.so-8.2610.0-0.daily202610${day}.84.1.gitc62d84ad7354.fc44.${arch}.debug`,
  `2026-10-${day}T10:14:43.4271807Z    /usr/lib64/rsyslog/omkinesis.so`,
  `2026-10-${day}T10:14:43.4271807Z `,
  `2026-10-${day}T10:14:44.6193231Z ##[error]Process completed with exit code 1.`,
].join('\n');

const cause = report.extractCause(unpackaged('aarch64', '16'));
assert.deepStrictEqual(cause, [
  'error: Installed (but unpackaged) file(s) found:',
  '/usr/lib/debug/usr/lib64/rsyslog/omkinesis.so-8.2610.0-0.daily20261016.84.1.gitc62d84ad7354.fc44.aarch64.debug',
  '/usr/lib64/rsyslog/omkinesis.so',
]);
assert.strictEqual(report.fingerprint(cause),
  report.fingerprint(report.extractCause(unpackaged('x86_64', '06'))));
assert.strictEqual(report.fingerprint(['b', 'a']), report.fingerprint(['a', 'b']));
assert.notStrictEqual(report.fingerprint(['ERROR: one']), report.fingerprint(['ERROR: other']));
assert.deepStrictEqual(report.extractCause([
  'ERROR: module /lib64/security/pam_pwquality.so is not installed.',
  'openSUSE baseline gained unreviewed patches; review them before packaging main: x.patch',
].join('\n')), ['openSUSE baseline gained unreviewed patches; review them before packaging main: x.patch']);
assert.deepStrictEqual(report.extractCause('setup\nlast words\n##[error]Process completed with exit code 2.'),
  ['setup', 'last words']);

function fakeGithub(logs) {
  const state = {issues: [], comments: [], calls: []};
  const rest = {
    actions: {
      listJobsForWorkflowRun: 'listJobs',
      downloadJobLogsForWorkflowRun: async ({job_id}) => ({data: logs[job_id]}),
    },
    issues: {
      listForRepo: 'listIssues',
      listComments: 'listComments',
      create: async ({title, body}) => {
        const issue = {number: state.issues.length + 1, title, body, user: {login: 'github-actions[bot]'}};
        state.issues.push(issue);
        state.calls.push('create');
        return {data: issue};
      },
      addLabels: async ({labels}) => { state.calls.push(`labels:${labels.join(',')}`); },
      update: async ({issue_number, body}) => {
        state.issues.find(issue => issue.number === issue_number).body = body;
        state.calls.push('update-issue');
      },
      createComment: async ({body}) => {
        state.comments.push({id: state.comments.length + 1, body, user: {login: 'github-actions[bot]'}});
        state.calls.push('comment');
      },
      updateComment: async ({comment_id, body}) => {
        state.comments.find(comment => comment.id === comment_id).body = body;
        state.calls.push('update-comment');
      },
    },
  };
  const paginate = async (method) => {
    if (method === 'listJobs') {
      if (logs === null) {
        throw new Error('jobs unavailable');
      }
      return Object.keys(logs).map(id => ({
        id, name: `build ${id}`, conclusion: 'failure',
        steps: [{name: 'build step', conclusion: 'failure'}],
      }));
    }
    if (method === 'listIssues') {
      return state.issues;
    }
    return state.comments;
  };
  return {github: {rest, paginate}, state};
}

const context = {repo: {owner: 'o', repo: 'r'}, serverUrl: 'https://example', sha: 'abc'};
const core = {warning: () => {}};
const options = {
  title: '[test-daily-stable] package archive failure', distro: 'Test',
  labels: ['daily-stable'], version: '1.0', results: {build: 'failure'},
};

(async () => {
  const first = fakeGithub({1: unpackaged('x86_64', '06'), 2: unpackaged('aarch64', '06')});
  await report({github: first.github, context: {...context, runId: 1}, core}, options);
  assert.deepStrictEqual(first.state.calls, ['create', 'labels:daily-stable']);
  assert.match(first.state.issues[0].body, /Likely cause:\n```text\nbuild 1 \(build step\):/);
  assert.match(first.state.issues[0].body, /\/usr\/lib64\/rsyslog\/omkinesis\.so/);
  assert.match(first.state.issues[0].body, /\nbuild 2: same cause as build 1\n```\n/);
  assert.match(first.state.issues[0].body, /Still failing for this cause as of .*runs\/1$/);

  // Same cause on another night and architecture order: update, no comment.
  const second = fakeGithub({2: unpackaged('aarch64', '16'), 1: unpackaged('x86_64', '16')});
  second.state.issues.push(first.state.issues[0]);
  await report({github: second.github, context: {...context, runId: 2}, core}, options);
  assert.deepStrictEqual(second.state.calls, ['update-issue']);
  assert.match(second.state.issues[0].body, /runs\/2$/);
  assert.strictEqual(second.state.issues[0].body.match(/Still failing/g).length, 1);

  // A new cause is a new comment; its repeat updates that comment only.
  const patchLog = 'ERROR: unexpected Debian patch failure \'x.patch\'\n##[error]Process completed';
  const third = fakeGithub({1: patchLog});
  third.state.issues.push(second.state.issues[0]);
  await report({github: third.github, context: {...context, runId: 3}, core}, options);
  assert.deepStrictEqual(third.state.calls, ['comment']);
  await report({github: third.github, context: {...context, runId: 4}, core}, options);
  assert.deepStrictEqual(third.state.calls, ['comment', 'update-comment']);
  assert.strictEqual(third.state.comments.length, 1);
  assert.match(third.state.comments[0].body, /runs\/4$/);

  // A quoted line with backticks gets a longer fence and cannot close it.
  const fence = fakeGithub({1: 'ERROR: tricky ``` @someone\n##[error]Process completed'});
  await report({github: fence.github, context: {...context, runId: 5}, core}, options);
  assert.match(fence.state.issues[0].body, /\n````text\n[^]*ERROR: tricky ``` @someone\n````\n/);

  // Only issues and comments created by the workflow token count; a user
  // issue with the same title is ignored.
  const impostor = fakeGithub({1: patchLog});
  impostor.state.issues.push({number: 9, title: options.title, body: '', user: {login: 'someone'}});
  await report({github: impostor.github, context: {...context, runId: 6}, core}, options);
  assert.deepStrictEqual(impostor.state.calls, ['create', 'labels:daily-stable']);
  const fakeComment = fakeGithub({1: patchLog});
  fakeComment.state.issues.push(third.state.issues[0]);
  fakeComment.state.comments.push({id: 1, body: third.state.comments[0].body, user: {login: 'someone'}});
  await report({github: fakeComment.github, context: {...context, runId: 7}, core}, options);
  assert.deepStrictEqual(fakeComment.state.calls, ['comment']);

  // An unreadable job list still files a report.
  const nojobs = fakeGithub(null);
  await report({github: nojobs.github, context: {...context, runId: 8}, core}, options);
  assert.deepStrictEqual(nojobs.state.calls, ['create', 'labels:daily-stable']);
  assert.match(nojobs.state.issues[0].body, /failure cause unavailable/);

  // Overlong log lines are truncated.
  assert.strictEqual(report.extractCause(`ERROR: ${'x'.repeat(5000)}`)[0].length, 406);
})().catch(error => {
  console.error(error);
  process.exit(1);
});
JS
