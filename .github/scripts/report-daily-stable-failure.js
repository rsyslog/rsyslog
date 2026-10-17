// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Rainer Gerhards and Others
//
// Create or update the per-distribution daily-stable failure issue.
//
// The report quotes the likely cause from the failed job logs, so the issue
// says what broke without opening the run.  A run failing for the same cause
// as the latest report updates that report's "still failing" line instead of
// adding another identical comment; a new cause gets a new comment.
//
// Used from actions/github-script:
//   const report = require('./.github/scripts/report-daily-stable-failure.js');
//   await report({github, context, core}, {title, distro, labels, version, results});

'use strict';

const crypto = require('crypto');

const CAUSE_PATTERNS = [
  /baseline gained unreviewed patches.*/,
  /reviewed baseline patch (?:changed|is missing).*/,
  /baseline patch is missing.*/,
  /could not (?:remove|find|disable|insert|add) .*/,
  /^ERROR: .*/,
  /^error: .*/,
  /^E: .*/,
  /^dh_[a-z_]+: error: .*/,
  /dpkg-buildpackage: error: .*/,
];
// Known harmless container setup output that matches the patterns above.
const NOISE_PATTERNS = [
  /pam_pwquality\.so is not installed/,
];
const UNPACKAGED_HEADER = /Installed \(but unpackaged\) file\(s\) found:/;
const MAX_CAUSE_LINES = 20;
const MAX_LINE_LENGTH = 400;
// Issues opened through GITHUB_TOKEN; anyone could open one with our title.
const BOT_LOGIN = 'github-actions[bot]';
const FINGERPRINT_MARK = 'daily-stable-cause:';

function stripTimestamp(line) {
  return line.replace(/^\d{4}-\d\d-\d\dT[\d:.]+Z ?/, '');
}

function extractCause(log) {
  const lines = log.split('\n').map(stripTimestamp);
  const cause = [];
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (UNPACKAGED_HEADER.test(line)) {
      cause.push(line.trim());
      for (let j = i + 1; j < lines.length && /^\s+\//.test(lines[j]); j++) {
        cause.push(lines[j].trim());
      }
      break;
    }
    if (CAUSE_PATTERNS.some(pattern => pattern.test(line)) &&
        !NOISE_PATTERNS.some(pattern => pattern.test(line))) {
      cause.push(line.trim());
    }
  }
  if (cause.length === 0) {
    // Fall back to what led up to the step failure marker.
    const end = lines.findIndex(line => line.startsWith('##[error]'));
    if (end > 0) {
      cause.push(...lines.slice(Math.max(0, end - 3), end).map(line => line.trim()));
    }
  }
  return cause.filter(Boolean).map(line =>
    line.length > MAX_LINE_LENGTH ? `${line.slice(0, MAX_LINE_LENGTH)} [...]` : line);
}

// Versions, hashes, timestamps, and architectures vary between runs and
// between matrix jobs; the cause does not.
function normalize(line) {
  return line
    .replace(/\b(?:x86_64|aarch64|amd64|arm64)\b/g, 'ARCH')
    .replace(/[0-9a-f]{7,}/g, '#')
    .replace(/\d+/g, '#');
}

function fingerprint(cause) {
  // Sort so the order in which jobs are listed is irrelevant.
  const normalized = [...new Set(cause.map(normalize))].sort().join('\n');
  return crypto.createHash('sha256').update(normalized).digest('hex').slice(0, 16);
}

async function collectCause(github, context, core) {
  const {owner, repo} = context.repo;
  const jobs = await github.paginate(github.rest.actions.listJobsForWorkflowRun, {
    owner, repo, run_id: context.runId, per_page: 100
  });
  const cause = [];
  const seen = new Map();
  for (const job of jobs.filter(job => job.conclusion === 'failure')) {
    const failedSteps = (job.steps || [])
      .filter(step => step.conclusion === 'failure')
      .map(step => step.name);
    let lines;
    try {
      const response = await github.rest.actions.downloadJobLogsForWorkflowRun({
        owner, repo, job_id: job.id
      });
      lines = extractCause(String(response.data));
      if (!lines.length) {
        lines = ['no cause line recognized; see the job log'];
      }
    } catch (error) {
      core.warning(`Could not read log of job ${job.name}: ${error.message}`);
      lines = ['job log unavailable'];
    }
    // Matrix jobs usually fail identically; quote such a cause only once.
    const key = lines.map(normalize).join('\n');
    if (seen.has(key)) {
      cause.push(`${job.name}: same cause as ${seen.get(key)}`);
      continue;
    }
    seen.set(key, job.name);
    cause.push(`${job.name}${failedSteps.length ? ` (${failedSteps.join(', ')})` : ''}:`, ...lines);
  }
  return cause.slice(0, MAX_CAUSE_LINES);
}

module.exports = async function report({github, context, core}, options) {
  const {owner, repo} = context.repo;
  const runUrl = `${context.serverUrl}/${owner}/${repo}/actions/runs/${context.runId}`;
  const version = options.version || `run-${context.runId}`;
  let cause;
  try {
    cause = await collectCause(github, context, core);
  } catch (error) {
    // Still report the failure when the job list cannot be read.
    core.warning(`Could not collect the failure cause: ${error.message}`);
    cause = ['failure cause unavailable; see the workflow run'];
  }
  const mark = `<!-- ${FINGERPRINT_MARK} ${fingerprint(cause)} -->`;
  const today = new Date().toISOString().slice(0, 10);
  const stillFailing = `Still failing for this cause as of ${today}: ${runUrl}`;
  // Log lines are quoted verbatim; a fence longer than any backtick run in
  // them keeps the lines from closing the block and injecting Markdown.
  const longestRun = Math.max(0, ...cause.map(line => Math.max(0, ...(line.match(/`+/g) || []).map(run => run.length))));
  const fence = '`'.repeat(Math.max(3, longestRun + 1));

  const body = [
    mark,
    `Automated ${options.distro} daily stable failed for \`${version}\`.`,
    '',
    `Workflow run: ${runUrl}`,
    `Workflow commit: ${context.sha}`,
    '',
    'Likely cause:',
    `${fence}text`,
    ...(cause.length ? cause : ['no failed job found; see the workflow run']),
    fence,
    '',
    'Job results:',
    ...Object.entries(options.results).map(([job, result]) => `- ${job}: ${result}`),
    '',
    stillFailing,
  ].join('\n');

  const existing = await github.paginate(github.rest.issues.listForRepo, {
    owner, repo, state: 'open', per_page: 100
  });
  const isBot = item => item.user && item.user.login === BOT_LOGIN;
  const issue = existing.find(item => item.title === options.title && !item.pull_request && isBot(item));
  if (!issue) {
    const created = await github.rest.issues.create({owner, repo, title: options.title, body});
    try {
      await github.rest.issues.addLabels({
        owner, repo, issue_number: created.data.number, labels: options.labels
      });
    } catch (error) {
      core.warning(`Could not add labels: ${error.message}`);
    }
    return;
  }

  // The latest report is the newest comment, or the issue itself.
  const comments = await github.paginate(github.rest.issues.listComments, {
    owner, repo, issue_number: issue.number, per_page: 100
  });
  const latest = [...comments].reverse()
    .find(comment => isBot(comment) && (comment.body || '').includes(FINGERPRINT_MARK));
  const latestBody = latest ? latest.body : (issue.body || '');
  if (latestBody.startsWith(mark)) {
    const updated = latestBody.replace(/\nStill failing for this cause as of .*$/, '') + `\n${stillFailing}`;
    if (latest) {
      await github.rest.issues.updateComment({owner, repo, comment_id: latest.id, body: updated});
    } else {
      await github.rest.issues.update({owner, repo, issue_number: issue.number, body: updated});
    }
    return;
  }
  await github.rest.issues.createComment({owner, repo, issue_number: issue.number, body});
};

module.exports.extractCause = extractCause;
module.exports.fingerprint = fingerprint;
