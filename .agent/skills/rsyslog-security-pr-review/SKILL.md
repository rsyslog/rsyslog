---
name: rsyslog-security-pr-review
description: Guide security review in the PR, optionally review a local delta, or fix an explicitly authorized security finding. Use for SECURITY REVIEW and SECURITY RESOLVE candidate-id; local AI review is not a readiness gate.
---

# rsyslog Security PR Review

Security review belongs in the PR through the available hosted review and
maintainer review process. Apply this guidance to the PR's current head and
address actionable security findings alongside other review feedback. Do not
assume that a hosted security reviewer is configured or has run: inspect and
report its actual status.

## Local review is optional

A local AI security review may supplement PR review when useful, but is not a
prerequisite for commits, pushes, PR creation, deterministic validation, FINISH,
or local completion. No particular reviewer model, input package, receipt, or
local pass status is required. Existing local review tools and ignored artifacts
may still be used as optional aids; their absence or stale status does not block
normal work.

Codex execution and approval restrictions can block local review even when the
reviewer uses Daybreak Blue. Model selection does not grant execution permission,
and repeated local review attempts can leave a session stuck. If optional local
review is blocked or unavailable, state that limit and continue with the normal
validation and PR review workflow. Never claim that an unavailable or incomplete
review passed.

Deterministic checks, tests, container validation, and CI remain required under
their own repository instructions. Actionable findings still need resolution;
removing a local reviewer prerequisite does not dismiss evidence of a defect.

## Review context and scope

Read `SECURITY.md`, `doc/ai/security_triage_rubric.md`, and the relevant parts of
`doc/security/project-threat-model.md` and
`doc/security/threat-model-components.json`. Inspect the current PR delta plus
the smallest supporting source needed to trace changed trust boundaries and
invariants. Do not turn an ordinary PR review into a full-repository scan.

For each candidate finding, record:

- changed path and lines, source, control, and sink;
- attacker capability, reachability, and demonstrated impact;
- evidence and counterevidence, including compatibility and existing guards;
- whether the PR introduces or worsens the issue, or the issue is pre-existing;
- proof gaps and disposition: confirmed, deferred, hardening, or not actionable.

Use direct code proof or a safe reproducer before treating a hypothesis as a
confirmed vulnerability. Keep severity proportional to demonstrated exposure
and follow the project's triage rubric. Stop when the reviewed delta has no
actionable findings; no multi-agent sequence or model-specific receipt is
required.

## PR follow-up and reporting

Follow `rsyslog_pr_babysitting` for the current head's checks, review threads,
and AI comments. Fix actionable findings within the authorized scope and reply
according to the review-thread rules. For findings needing a maintainer decision,
report the precise issue and evidence instead of silently accepting the risk.

Report local validation and PR security review separately. Say which review ran,
which head it covered, its findings, and what remains pending or unavailable.
Passing CI or a zero-finding optional local pass is not an exhaustive security
audit or proof that the PR review has completed.

## Fix an explicitly authorized finding

`SECURITY RESOLVE candidate-id` authorizes fixing that finding, not unrelated
candidates. Confirm the finding against the current source, implement the
smallest complete compatible fix, and add meaningful regression coverage that
fails without the fix plus a legitimate-input control where applicable. Run the
relevant reproducer, closest tests, formatting, build, and container checks.
Review the final change in the PR; an optional independent local review may help,
but unavailable models or local security receipts must not stall the fix loop.

Follow `SECURITY.md` for confidential vulnerability reporting and coordinated
disclosure. Keep private findings, credentials, embargoed details, and exploit
artifacts out of public PR comments and tracked files until disclosure is
authorized. Use the approved private review channel for those cases.
