// SPDX-License-Identifier: Apache-2.0
// Contract regressions for the trusted consumer. Fixtures represent the API
// boundary: rejected input must never produce an upload target. Stale heads
// are a normal no-upload result, while inconsistent provenance is an error.
const assert = require('node:assert/strict');
const resolve = require('../devtools/pr-coverage-source.cjs');

function fixture(sameRepo = false, docs = false) {
  const base = {id: 1, full_name: 'rsyslog/rsyslog', owner: {login: 'rsyslog'}};
  const fork = sameRepo ? base : {id: 2, full_name: 'alice/rsyslog', owner: {login: 'alice'}};
  const run = {id: 41, workflow_id: 12, repository: base, event: 'pull_request', status: 'completed',
    run_attempt: 2, head_sha: 'a'.repeat(40), head_branch: 'fix/topic', head_repository: fork,
    html_url: 'https://github.com/rsyslog/rsyslog/actions/runs/41', conclusion: 'failure'};
  return {event: JSON.parse(JSON.stringify(run)), run,
    workflow: {path: docs ? '.github/workflows/doc_build.yml' : '.github/workflows/run_checks.yml',
      name: docs ? 'documentation build' : 'check'},
    prs: [{number: 72, state: 'open', base: {repo: base, ref: 'main'},
      head: {repo: JSON.parse(JSON.stringify(fork)), ref: run.head_branch, sha: run.head_sha}}],
    jobs: [{name: docs ? 'build_docs' : 'CI (ubuntu_24)', conclusion: 'success',
      steps: (docs ? ['Validate documentation config samples', 'Generate documentation sample coverage',
        'Preserve documentation sample coverage for the trusted uploader'] :
        ['Capture representative PR coverage', 'Preserve coverage for the trusted uploader'])
        .map(name => ({name, conclusion: 'success'}))}],
    artifacts: [{id: 81, name: docs ? 'coverage-docs-samples-pr-41-2' : 'coverage-ubuntu-24-pr-41-2', expired: false, size_in_bytes: 200,
      workflow_run: {id: 41, head_sha: run.head_sha}}]};
}

async function evaluate(data) {
  const outputs = {};
  const actions = {
    getWorkflowRun: async args => { assert.equal(args.run_id, 41); return {data: data.run}; },
    getWorkflow: async args => { assert.equal(args.workflow_id, 12); return {data: data.workflow}; },
    listJobsForWorkflowRunAttempt: 'jobs', listWorkflowRunArtifacts: 'artifacts'
  };
  const github = {rest: {actions, pulls: {list: 'prs'}}, paginate: async (kind, args) => {
    assert.equal(args.owner, 'rsyslog');
    assert.equal(args.repo, 'rsyslog');
    if (kind === 'jobs') { assert.equal(args.attempt_number, 2); assert.equal(args.run_id, 41); }
    if (kind === 'artifacts') assert.equal(args.run_id, 41);
    if (kind === 'prs') assert.equal(args.state, 'open');
    return data[kind];
  }};
  await resolve({github, context: {repo: {owner: 'rsyslog', repo: 'rsyslog'},
    payload: {workflow_run: data.event}}, core: {notice() {}, setOutput(key, value) { outputs[key] = value; }}});
  return outputs;
}

(async () => {
  // Fork branch attribution and PR head SHA must not inherit the consumer's
  // default-branch context. Other matrix failures must not suppress this lane.
  for (const docs of [false, true]) {
    for (const sameRepo of [false, true]) {
      const output = await evaluate(fixture(sameRepo, docs));
      assert.equal(output.sha, 'a'.repeat(40));
      assert.equal(output.pr, 72);
      assert.equal(output.artifact, 81);
      assert.equal(output.branch, sameRepo ? 'fix/topic' : 'alice:fix/topic');
      assert.equal(output.flag, docs ? 'docs-samples' : 'pr-ubuntu-24');
      assert.equal(output.kind, docs ? 'docs' : 'runtime');
      assert.equal(output.check, docs ? 'PR coverage upload (docs-samples)' : 'PR coverage upload (pr-ubuntu-24)');
    }
  }
  const rejected = {
    'wrong repository': d => { d.run.repository.full_name = 'mallory/rsyslog'; },
    'wrong workflow path': d => { d.workflow.path = '.github/workflows/other.yml'; },
    'wrong workflow name': d => { d.workflow.name = 'other'; },
    'wrong event': d => { d.run.event = 'push'; },
    'wrong run': d => { d.run.id = 42; },
    'wrong attempt': d => { d.run.run_attempt = 3; },
    'wrong SHA': d => { d.run.head_sha = 'b'.repeat(40); },
    'wrong branch': d => { d.run.head_branch = 'other'; },
    'wrong head repository': d => { d.event.head_repository.id = 8; },
    'unsafe branch': d => { d.run.head_branch = d.event.head_branch = 'fix\nINJECT=x'; },
    'missing artifact': d => { d.artifacts = []; },
    'duplicate artifact': d => { d.artifacts.push({...d.artifacts[0], id: 82}); },
    'stale artifact attempt': d => { d.artifacts[0].name = 'coverage-ubuntu-24-pr-41-1'; },
    'expired artifact': d => { d.artifacts[0].expired = true; },
    'oversized artifact': d => { d.artifacts[0].size_in_bytes = 10485761; },
    'foreign artifact run': d => { d.artifacts[0].workflow_run.id = 42; },
    'foreign artifact SHA': d => { d.artifacts[0].workflow_run.head_sha = 'b'.repeat(40); }
  };
  for (const docs of [false, true]) {
    for (const [name, change] of Object.entries(rejected)) {
      const data = fixture(false, docs); change(data);
      await assert.rejects(evaluate(data), undefined, name);
    }
  }
  const skipped = {
    'stale PR head': d => { d.prs[0].head.sha = 'b'.repeat(40); },
    'closed PR': d => { d.prs[0].state = 'closed'; },
    'other PR repo': d => { d.prs[0].head.repo.id = 9; },
    'wrong PR base': d => { d.prs[0].base.ref = 'v8-stable'; },
    'ambiguous PR': d => { d.prs.push(d.prs[0]); },
    'failed runtime lane': d => { d.jobs[0].conclusion = 'failure'; },
    'missing coverage job': d => { d.jobs = []; },
    'generation skipped (docs_changed false)': d => {
      d.jobs[0].steps[1].conclusion = 'skipped'; d.artifacts = [];
    },
    'missing generation step': d => { d.jobs[0].steps = []; },
    'wrong generation step': d => { d.jobs[0].steps[1].name = 'Unrelated successful step'; }
  };
  for (const docs of [false, true]) {
    for (const [name, change] of Object.entries(skipped)) {
      const data = fixture(false, docs); change(data);
      assert.deepEqual(await evaluate(data), {}, name);
    }
  }
  // A producer cannot select a different route/flag via artifact metadata.
  for (const docs of [false, true]) {
    const forged = fixture(false, docs);
    forged.artifacts[0].flag = docs ? 'pr-ubuntu-24' : 'docs-samples';
    assert.equal((await evaluate(forged)).flag, docs ? 'docs-samples' : 'pr-ubuntu-24');
    forged.artifacts[0].name = docs ? 'coverage-ubuntu-24-pr-41-2' : 'coverage-docs-samples-pr-41-2';
    await assert.rejects(evaluate(forged));
    const wrongWorkflow = fixture(false, docs);
    wrongWorkflow.workflow.name = docs ? 'check' : 'documentation build';
    await assert.rejects(evaluate(wrongWorkflow));
  }
  console.log('PASS: PR runtime/docs coverage identity and stale/provenance refusal contracts');
})().catch(error => { console.error(error); process.exitCode = 1; });
