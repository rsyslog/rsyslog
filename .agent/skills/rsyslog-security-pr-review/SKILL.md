---
name: rsyslog-security-pr-review
description: Run rsyslog's local, threat-model-aware security delta review for a PR candidate, create a digest-bound local receipt, or resolve one independently confirmed candidate. Use for `$rsyslog-security-pr-review`, `SECURITY REVIEW`, `SECURITY RESOLVE candidate-id`, and FINISH on code PR work.
---

# rsyslog Security PR Review

Review only the stabilized local PR delta against the reusable project model.
Keep the normal lane small: deterministic routing, one discovery pass, and no
further agents when no candidate exists. Never call an external service from a
repository script, publish artifacts, or edit code during the normal review.

Local artifacts live under ignored `.codex/security-review/`. Never put a PoC,
credential, embargoed fact, private advisory reference, or unresolved exploit
instruction in a tracked file or public summary.

## Required context and input

Read, in order:

1. `SECURITY.md`
2. `doc/ai/security_triage_rubric.md`
3. `doc/security/project-threat-model.md`
4. `doc/security/threat-model-components.json`

Then build the complete candidate package:

```sh
python3 devtools/build-security-review-input.py
```

The builder is the authority for the base, changed-file inventory, digest,
component coverage, and route. It includes committed, staged, unstaged, and
untracked changes. Do not substitute `git diff` by itself. Pass `--base REF`
only when the PR base is intentionally different from the repository's local
validation base.

## Review routing

- `not_applicable`: write a minimal receipt and stop. Do not start discovery.
- `quick`: use one fresh Terra/medium discovery agent. Review every path in
  `coverage.expected_files` and only directly supporting code needed to trace
  the mapped boundaries and invariants.
- `lead_required`: run the same bounded discovery, then give the package and
  results to a fresh Terra/high security lead. Model changes, unmapped source,
  high-risk components, and ambiguous trust-boundary changes use this route.
- `expanded`: split at most once into two non-overlapping discovery shards,
  with every expected file assigned exactly once. If two bounded shards cannot
  cover the delta, write `needs_human`; do not truncate or silently exceed the
  lane.

If a required model or a fresh independent role is unavailable, write
`needs_human`; do not silently reuse the discovering agent or substitute a
different role.

Use the installed Codex Security Diff Scan and Finding Discovery phase
guidance when available. They are phase guidance, not an instruction to run a
full-repository or deep scan. If unavailable, use this equivalent sequence:

1. Confirm every expected file is assigned exactly once.
2. Inspect changed lines plus the smallest source/control/sink context.
3. Test only mapped trust boundaries and invariants, plus obvious newly created
   boundaries.
4. Record candidate evidence and counterevidence without severity inflation.
5. Stop immediately when there are no candidates.

Target less than two minutes and 25,000 aggregate tokens for ordinary
discovery. Do not run validation, attack-path analysis, or propose fixes for a
zero-candidate review.

## Candidate validation and lead triage

Always write `.codex/security-review/candidates.json`, including an empty
`candidates` array for a zero-candidate review. Include `schema_version`, the
input digest and model revision, discovery coverage, and the array. Each
candidate needs a stable local ID, introduced-or-worsened status, affected
files and model IDs, source, control, sink, attacker capability, impact,
counterevidence, proof gaps, discovery disposition, and timestamps. Candidate
discovery alone never blocks.

Give candidates to a fresh Terra/medium validator that did not perform
discovery. Follow the Codex Security Validation phase when available. Require
direct code proof or a safe reproducer of attacker control, reachability, and
impact. Classify each result as `confirmed`, `deferred`, `hardening`, or
`not_actionable`; retain proof gaps and separate pre-existing findings from
PR-introduced or worsened findings.

The discovery-plus-validation target is less than five minutes and 60,000
aggregate tokens. Invoke a fresh Terra/high security lead only for confirmed or
deferred candidates, model changes, unmapped security-relevant code, or an
ambiguous boundary change. Attack-path analysis is conditional: use it only
when source-to-sink reachability or severity remains material and unclear.

Blocking policy:

- Confirmed PR-introduced or worsened Critical, High, or Medium findings:
  `blocked`.
- Confirmed Low: warn and normally `passed`.
- Serious unresolved High/Critical hypothesis: `needs_human`.
- Discovery-only, hardening, not-actionable, and unrelated pre-existing
  results: do not block; report them separately and precisely.

## Receipt

Write `.codex/security-review/receipt.json` only after coverage and disposition
are known. Include at least:

- `schema_version`, `input_schema_version`, `diff_digest`, `model_revision`
- `route`, `coverage.expected_files`, `coverage.reviewed_files`, and whether
  coverage is complete
- candidate counts by disposition and introduced-or-worsened status
- `started_at`, `completed_at`, elapsed time, agent roles used, and approximate
  aggregate tokens
- final `status`: `passed`, `blocked`, `needs_human`, or `not_applicable`

A receipt is current only if a freshly rebuilt `input.json` has the same
`diff_digest` and model revision, `reviewed_files` has no duplicates, and its
set exactly equals the rebuilt `expected_files` set. Never reuse a stale
receipt. `FINISH` and agent-led container completion require a current `passed`
or `not_applicable` receipt for code PR work. This comparison is performed by
the agent; no shell or CI hook enforces it.

## Resolve one confirmed candidate

Run this section only for `$rsyslog-security-pr-review resolve <candidate-id>`
or `SECURITY RESOLVE <candidate-id>`. This explicit invocation authorizes one
fix, not other candidates. Refuse if the candidate is absent, not independently
confirmed, or its stored digest differs from a freshly built input.

1. Give a fresh Terra/medium agent a read-only boundary and compatibility
   investigation before editing.
2. Assign Luna/xhigh as the only writing agent. It implements the smallest
   complete repository-native fix, a regression test that fails without it,
   and a legitimate control proving compatible behavior remains.
3. Run the reproducer, a distinct malicious-input class, the legitimate
   control, closest tests, and applicable rsyslog build checks. Follow the
   repository build, test, formatting, and test-comment skills.
4. Give a fresh Terra/medium verifier only the original finding, repository
   policy, and candidate diff. Do not give it the patch rationale or earlier
   conclusions. It must reconstruct the invariant, search for bypasses and
   regressions, and verify that the original path is closed. Use Codex Security
   Verify Fix guidance when available.
5. Rebuild `input.json`, rerun this compact review over the final digest, and
   replace the receipt only after that digest passes.
6. Run PR-ready local container validation after the fix loop stabilizes.

Do not auto-fix during `SECURITY REVIEW`. If the specified writer model is not
available, stop with `needs_human`; do not silently replace the only authorized
writing role.

## Updating the baseline

Update the model revision and component map together when the PR adds or
materially changes a trust boundary, security invariant, attacker capability,
default exposure, privileged operation, executable/module loading path,
persistent-state authority, or release trust path. Add component-local evidence
and focused test hints when a new subsystem appears. Routine internal refactors
that preserve the mapped boundary and invariants do not require model churn.

For a model update, obtain one fresh read-only architecture review, then
independently verify every material claim and repository-relative citation.
Keep attacker stories as hypotheses, remove volatile implementation trivia,
and never turn an unresolved suspicion into a repository claim.
