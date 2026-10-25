// SPDX-License-Identifier: Apache-2.0
// Resolve upload identity exclusively from GitHub API data, never artifact contents.
module.exports = async function resolveCoverageSource({github, context, core}) {
  // These profiles are trusted code. Artifact content cannot choose its
  // workflow, upload flag, parser source root, or PR check identity.
  const profiles = [
    {path: '.github/workflows/run_checks.yml', name: 'check', job: 'CI (ubuntu_24)',
      prefix: 'coverage-ubuntu-24-pr', flag: 'pr-ubuntu-24', kind: 'runtime',
      steps: ['Capture representative PR coverage', 'Preserve coverage for the trusted uploader']},
    {path: '.github/workflows/doc_build.yml', name: 'documentation build', job: 'build_docs',
      prefix: 'coverage-docs-samples-pr', flag: 'docs-samples', kind: 'docs',
      steps: ['Validate documentation config samples', 'Generate documentation sample coverage',
        'Preserve documentation sample coverage for the trusted uploader']}
  ];
  const event = context.payload.workflow_run;
  const repo = context.repo;
  const {data: run} = await github.rest.actions.getWorkflowRun({...repo, run_id: event.id});
  const {data: workflow} = await github.rest.actions.getWorkflow({...repo, workflow_id: run.workflow_id});
  const profile = profiles.find(item => item.path === workflow.path && item.name === workflow.name);
  if (!profile || run.id !== event.id || run.repository.full_name !== 'rsyslog/rsyslog' || run.event !== 'pull_request' ||
      run.status !== 'completed' || run.run_attempt !== event.run_attempt ||
      run.head_sha !== event.head_sha || run.head_branch !== event.head_branch ||
      run.head_repository?.id !== event.head_repository?.id ||
      !/^[0-9a-f]{40}$/.test(run.head_sha)) {
    throw new Error('Unexpected coverage source workflow/run');
  }
  // Branch names come from GitHub and remain API/action data. Reject control
  // characters that could split output records without excluding legal refs.
  const owner = run.head_repository?.owner?.login;
  if (!owner || !/^[a-zA-Z0-9-]+$/.test(owner) ||
      typeof run.head_branch !== 'string' || !run.head_branch || /[\x00-\x1f\x7f]/.test(run.head_branch)) {
    throw new Error('Unsafe or missing source branch metadata');
  }
  // workflow_run.pull_requests can be empty for fork PRs. Resolve via
  // the API, then match all identity fields instead of trusting a
  // PR number/commit supplied inside the artifact.
  const candidates = await github.paginate(github.rest.pulls.list, {
    ...repo, state: 'open', head: `${owner}:${run.head_branch}`, per_page: 100
  });
  const matches = candidates.filter(pr => pr.state === 'open' && pr.base.repo.full_name === 'rsyslog/rsyslog' &&
    ['main', 'v8-stable'].includes(pr.base.ref) && pr.head.sha === run.head_sha &&
    pr.head.ref === run.head_branch && pr.head.repo?.id === run.head_repository.id);
  if (matches.length !== 1) {
    core.notice('No unique current PR head; stale/closed runs are not uploaded');
    return;
  }
  const pr = matches[0];
  const jobs = await github.paginate(github.rest.actions.listJobsForWorkflowRunAttempt, {
    ...repo, run_id: run.id, attempt_number: run.run_attempt, per_page: 100
  });
  const lane = jobs.filter(job => job.name === profile.job);
  if (lane.length !== 1 || lane[0].conclusion !== 'success' ||
      profile.steps.some(name => !lane[0].steps?.some(step => step.name === name && step.conclusion === 'success'))) {
    core.notice('Coverage-producing job/steps did not pass or were skipped; no coverage upload');
    return;
  }
  const artifacts = await github.paginate(github.rest.actions.listWorkflowRunArtifacts, {
    ...repo, run_id: run.id, per_page: 100
  });
  const name = `${profile.prefix}-${run.id}-${run.run_attempt}`;
  const reports = artifacts.filter(artifact => artifact.name === name);
  if (reports.length !== 1 || reports[0].expired ||
      reports[0].size_in_bytes <= 0 || reports[0].size_in_bytes > 10485760 ||
      reports[0].workflow_run?.id !== run.id ||
      reports[0].workflow_run?.head_sha !== run.head_sha) {
    throw new Error('Missing, expired, duplicate, oversized or mismatched coverage artifact');
  }
  core.setOutput('flag', profile.flag);
  core.setOutput('kind', profile.kind);
  core.setOutput('check', `PR coverage upload (${profile.flag})`);
  core.setOutput('sha', run.head_sha);
  core.setOutput('pr', pr.number);
  // Namespace fork branches even with a token; never attribute fork
  // data to main or another protected baseline branch.
  core.setOutput('branch', pr.head.repo.id === pr.base.repo.id ? pr.head.ref : `${owner}:${pr.head.ref}`);
  core.setOutput('artifact', reports[0].id);
  core.setOutput('url', run.html_url);
};
