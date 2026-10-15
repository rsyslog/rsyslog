<!-- SPDX-License-Identifier: Apache-2.0 -->
# Transactional reload implementation and PR-stack ledger

Recorded: 2026-10-15. Status: first five dependent draft PRs published and
integrated with current main; slice 6 construction and focused checks ongoing.
Umbrella: https://github.com/rsyslog/rsyslog/pull/7654 (draft).

## Method and merge contract

- Use durable sibling worktrees; never depend on /tmp for work or evidence.
- Split by complete functionality and ownership invariant, not patch size alone.
- Each PR targets its immediate predecessor; merge strictly in numbered order.
  After its predecessor merges, retarget the next PR to main and verify that
  its reviewed delta is unchanged. Never merge a child before its parent.
- Fold follow-up fixes and their regression tests into the PR introducing the
  affected capability. Preserve SPDX/provenance; all new files use Apache-2.0.
- A candidate split is not accepted until it builds, its focused tests pass,
  and unsupported changes fail closed. No intermediate PR may expose known
  unsafe listener topology support that a later PR happens to repair.
- Keep each security delta within the bounded review lane; split again if a
  coherent unit exceeds it. Review with independent Luna/high roles.
- Preserve combined content with a final tree comparison against the rebased,
  hardened umbrella. Explain every intentional difference in this ledger.
- Every PR description must link parent, child, umbrella, and full merge order;
  state newly supported behavior, retained restrictions, and actual validation.
  Do not carry final-branch capability claims into an earlier stack member.
- Start every PR description with prominent merge requirements: parent first,
  retarget to main after parent merge, verify the delta, pass relevant checks
  and review, and never merge a child into an unmerged parent as a substitute.
- This umbrella-branch ledger is the canonical live record linked by every PR.
  Earlier slices contain historical snapshots; copy the final current ledger
  into the assembled stack tip so the merged series retains the final map.
- Maintainer-approved validation scope (2026-10-15): focused builds/tests and
  independent review for each intermediate PR; broad container validation on
  the final combined tip. Intermediate slices are targeted-tested, not fully
  PR-ready container-validated. Preserve any earlier broad-run failures.

## Implementation inventory at the umbrella checkpoint

These rows locate implemented code; they do not claim it has merged to main.

| Area | Implementation | Evidence / current limit |
| --- | --- | --- |
| Module lifecycle and graph reports | runtime/module-reload.c, runtime/modules.h, runtime/reload-normalized-graph.*, runtime/reload-report.* | Module/report/graph units; conservative missing-hook classification |
| Candidate loading and source identity | runtime/reload-candidate.*, runtime/reload-ruleset-graph.*, runtime/rsconf.*, grammar/parserif.h, runtime/yamlconf.* | RS/YAML parity and candidate graph tests; private loading rather than full reinitialization |
| Ruleset cloning and activation | grammar/reload-syntax-clone.c, grammar/reload-ruleset-materializer.*, runtime/ruleset.*, runtime/action.c | Clone/materializer tests; bounded bodies and unchanged named actions, no general call/action lifecycle |
| Queue and worker quiescence | runtime/queue-barrier.c, runtime/wtp-quiesce.c, runtime/wti.c, runtime/wtp.c | Worker units and closed-dequeue-window integration; keep pre-wait and idle rechecks |
| Existing TCP listener policy | plugins/imtcp/imtcp.c, runtime/tcpsrv.*, runtime/tcps_sess.* | RS/YAML ruleset, ACL, capacity, rate-limit and profile tests; accept-time profiles remain frozen |
| Prepared TCP endpoint lifecycle | runtime/tcpsrv-prepared.h, runtime/tcpsrv-poll.h, plugins/imtcp/imtcp-lifecycle.h | Production backend/lifecycle units and add/retire/replace tests; bounded plain TCP only |
| Coordinator and retirement | tools/shadow_reload.*, tools/rsyslogd.c | Main-loop pending cleanup retry; failed resume retains ownership and cannot retroactively reject published state |
| Global scalar policy | runtime/glbl.*, runtime/parser_sanitize_helper.h, runtime/msg_programname_helper.h | Parser/global RS/YAML tests; only one supported changed scalar per candidate |
| Operator documentation | doc/source/development/transactional_config_reload.rst, doc/source/rainerscript/global.rst, doc/source/configuration/modules/imtcp.rst | Distinguishes supported subset from future roadmap |
| Performance harness | benchmarks/imtcp-ruleset-reload/ | Existing validate-mode harness is not on-mode activation acceptance |

## Proposed functional merge order

All rows remain **planned** until the adjusted boundary is validated and a PR
URL/head/base is entered. Historical cut points are reconstruction aids, not
an assertion that those intermediate revisions are safe to merge unchanged.

| Order | Boundary | Historical endpoint | Required adjustments before publication |
| --- | --- | --- | --- |
| 1 | Opt-in HUP policy foundation and diagnostics | 4cec00ed2 | Keep activation unsupported; distinguish benchmark tooling from acceptance |
| 2 | Deterministic reports and owned active graph | 6cd550a92 | Fold applicable graph ownership/error fixes |
| 3 | Side-effect-free candidate loading and source report | 4b19f5e2c | Preserve source isolation and fail-closed activation boundary |
| 4 | Queue barrier and owned existing-ruleset activation | 5b4bb32b4 | Fold selector identity/cache and worker quiescence merge-resolution fixes and tests |
| 5 | Detached imtcp candidate and endpoint ownership | fc0ab37b0 | No live topology publication; preserve effective-value classification |
| 6 | Compatible live policy and accept-time session profiles | df572f55d | Fold resume-error ownership/retry handling before exposing failing resume paths |
| 7 | Runtime reconciliation/retirement and rate-limit base | 1494de8ba | Gate unsafe topology before prepare until bounded lifecycle PR 12 |
| 8 | Capacity, sender ACLs and helper failure contracts | eb2f4d101 | Include denied-session reclamation in the ACL-introducing PR |
| 9 | Simple named rate-limit policy lifecycle | 1a6c3daa8 | Retain source ownership and sharing restrictions |
| 10 | Global/parser policy subset | 60dbf92ca | Move unrelated child-exit/scalar work here where practical; retain one-scalar rule |
| 11 | Remaining scalar profiles and coherent sampling | b2e01e773 | Fold parser helper/sampling fixes into earliest dependent scalar PR |
| 12 | Bounded prepared TCP add/remove/replace | 0d85d74e3 | Include poll capacity, readiness, startup serialization, diagnostics and docs together |

## Current evidence and remaining work

Published umbrella head before rebase: 75f8c832cb6901f0b642cad878b393fec858815b.
Backup: codex/reload-before-rebase-20261015.
Rebase base: 62ab7a35ff8dfe0fd572c6a39853b2b32d9b9e8f.
Local rebased head before folded fixes: 0d85d74e3.

Historical merge 7ef92ae48 carried nontrivial resolutions. Linear rebase omitted
some of them; restored small deltas cover selector identity/fingerprints,
cache reset, grammar cleanup, idle/quiesce checks, and their regression tests.
The upstream zero-length parser guard is retained in the extracted helper.

Post-rebase focused container run: 18 passed, zero skips/failures/errors,
including module resume helper, restored selector and closed-window tests,
production TCP backend and both replacement frontends. This is targeted
evidence, not fresh full-branch clearance. Independent Luna/high integration
review found no remaining concrete defect in the reviewed restoration/fix.

Durable local evidence: .codex/security-review/rebase-20261015.md,
rebase-range-diff.log, original-merge-resolution.patch,
rebase-focused-restored.log and rebase-format-final.log.

Rebased umbrella c779824606fb8095c48e52c5ee6f933d72301043 was published with
an explicit lease against the old head; GitHub then reported mergeable. Fresh broad
Ubuntu 26.04 evidence: 1,667 passes, 70 skips, zero failures/errors (863.94s).
Production analyzer: no bugs (267.35s). The recorder sealed the actual source
commit and original Automake summary; no publication override was needed.
Current whole-branch security package has 172 reviewable files and 25,406
changed reviewable lines: needs_human, to be replaced by per-slice review.

## Published dependent stack

Merge order: [1 / #7683](https://github.com/rsyslog/rsyslog/pull/7683) ->
[2 / #7684](https://github.com/rsyslog/rsyslog/pull/7684) ->
[3 / #7685](https://github.com/rsyslog/rsyslog/pull/7685) ->
[4 / #7686](https://github.com/rsyslog/rsyslog/pull/7686) ->
[5 / #7687](https://github.com/rsyslog/rsyslog/pull/7687) -> remaining slices.
All five are draft, assigned to rgerhards, milestone v8.2610, categories
configuration/enhancement. Each child targets the preceding feature branch.
Do not merge umbrella #7654 in parallel with this replacement stack.

| Slice / PR | Published opening head / base | Functional checks before latest main integration | Review and other evidence |
| --- | --- | --- | --- |
| 1 / #7683 | 557693312 / main | 8/8 policy/unit tests (104.58s); corrected checkpoint intervals 0/1/5 all pass (7.22s) | Independent Luna/high discovery and lead covered all 34 expected paths; earlier analyzer, no-systemd, mock distcheck, HTML and extended docs distribution passed |
| 2 / #7684 | 6b7e5ddd7 / codex/reload-stack-01-foundation | 7/7 report/graph/frontend checks (100.24s); both field-omission negative controls fail, restored control passes | Independent discovery, lead and validation completed; diagnostic concern classified non-blocking hardening; mock distribution and HTML passed |
| 3 / #7685 | 36b6722fa / codex/reload-stack-02-reports | 10/10 candidate/graph/frontend/error checks (100.09s) | Two Luna/high shards covered all 21 expected paths plus parser/lexer context; mock distribution and HTML passed |
| 4 / #7686 | 097f45f8f / codex/reload-stack-03-candidates | 12/12 clone/barrier/worker/activation checks (100.40s) | Two Luna/high shards covered all 36 expected paths; HTML passed; specialist sanitizer/distribution evidence remains distinct |
| 5 / #7687 | 99ded8113 / codex/reload-stack-04-ruleset-activation | 9/9 detached-candidate/frontend/TCP checks (55.58s) | Two Luna/high shards covered all 17 expected paths; HTML passed; permitted-peer allocation cleanup folded here, static error-path audit, no forced-OOM runtime claim |

These opening heads preserve provenance; current-main integration will change
them and requires refreshed evidence. Each slice contains one functional commit
with applicable follow-up fixes folded in. Never infer merge readiness from
draft publication or the table alone. Hosted CI and hosted AI review are
pending, not equivalent to the independent local reviews recorded here.

Slice 1's additional broad Ubuntu 26.04 run completed in 789.98 seconds:
1,595 passes, 70 skips, one failure, zero errors. The failing test is
segmented-diskqueue-checkpoint-5; its original log is retained. An isolated
rerun on the same source and pinned image failed identically: 15 physical
records (three startup records plus 12 injected messages), three periodic
writes versus the test's expected two. This is reproducible, not an accepted
flake or a clean broad-run result. The exact startup-residue oracle correction
is now folded into slice 1, with all three interval checks passing. Original
failure evidence is retained; the broad suite has not been rerun after this
correction. Earlier focused, analyzer and distribution results remain separate.
The image was rsyslog/rsyslog_dev_base_ubuntu:26.04, ID
sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09;
the run used devtools/run-ci.sh, GCC -g, build -j20 and checks -j10 with
default service relevance. Exact command/log provenance remains in the
worktree's ignored .codex/security-review/ directory. Refreshed focused lanes
use GCC -g and build/check -j10 for slices 1-4; slice 5 uses build -j60,
check -j20. HTML builds used doc/tools/build-doc-linux.sh --clean --format
html --jobs 10. These commands do not constitute a broad runtime gate.

The normal branch-push guard rejected slice 1 because it has no broad-pass
marker. No marker was fabricated. The maintainer explicitly authorized the
intermediate-branch SKIP_CONTAINER_VALIDATION=1 override separately from
focused-test scope; it was used for the five published feature branches only.
The final combined tip still requires the broad container gate.

Upstream advanced from the validated rebase base to ed0094984 while slicing;
the stack was refreshed onto it, retaining imtcp regex-framing and fuzz-corpus
whitespace fixes. Subsequently main advanced to c62d84ad7, merging PR #7593's
local queue implementation. The initial conflict is resolved; the complete
five-branch chain was published atomically with exact leases. GitHub verifies
all five are conflict-free against their declared bases, with one functional
commit each. Local queue ownership/shutdown behavior is preserved; active
local/mixed graph activation rejects explicitly before cloning or barriers.
All five current security packages have complete, unique reviewed-file coverage
and passed receipts against freshly rebuilt clean inventories. Hosted checks
and review remain separate merge requirements, not implied by mergeability.

Boundary adjustments already made: benchmark assets are absent from the first
three slices, retained in the umbrella for later performance/tooling work.
The disk-queue checkpoint-residue oracle correction is folded into slice 1
after its broad and isolated runs demonstrated the intermediate failure; no
later PR is needed to repair that test. It is not introduced again in slice 2.
The early graph producer is source-spelling-sensitive: case-only names can be
reported more strictly than runtime's case-insensitive ruleset lookup. This is
a conservative report limitation, not a security finding or semantic-equivalence
claim; expose the restriction in the affected PR's support statement.

Open work: construct and validate each adjusted stack boundary, record PR URLs
and exact source/image evidence, review security per slice, and complete the
activation-mode release-performance gate. Only then begin milestone 4 (general
ruleset graph/call lifecycle). Current bounded reload is not full-config reload.

Next boundary in progress: slice 6, bounded compatible existing-listener live
policy and accept-time profiles. First-introduction fixes include poll capacity
and failed-resume ownership/automatic retry before candidate disposal. It must
not enable listener addition/removal/replacement. Construction has resumed on
the integrated parent; module/input-only changes must also reject active
local/mixed graphs before any module preparation or publication.

Current-main integration results: slices 1-3 are published on
c62d84ad7 at f21fdc83c / 2d05b707f / ba2f993cf. Their refreshed focused
lanes passed 13/13 (71.38s), 9/9 (63.89s) and 12/12 (64.00s), including
RainerScript/YAML local-queue configuration controls and all three checkpoint
intervals in slice 1. Fresh independent security reviews cover those exact
deltas; receipts are sealed against the rebuilt clean inventories.

| PR | Current published head | Verified base |
| --- | --- | --- |
| #7683 | f21fdc83cb2075540c7179332f6cc07ee6ccf716 | main / c62d84ad7 |
| #7684 | 2d05b707f03c600cd4f93d86851f77ec4360ecd4 | #7683 / f21fdc83c |
| #7685 | ba2f993cfac69c93f2462e27bc4d131fb7dcbe45 | #7684 / 2d05b707f |
| #7686 | 33aeada71a5077156d9eb43f241db2e863767873 | #7685 / ba2f993cf |
| #7687 | 659a3cbf8874b8e02af56de5f5cbff27aa8f827e | #7686 / 33aeada71 |

Slice 4's six final RS/YAML controls passed: global activation positives plus
local-only and mixed-graph rejection with old generation, old routing and the
configured-omfile cause verified. Eight units, four critical local lifecycle
controls and the closed-dequeue regression passed. Mock distcheck passed with
1,577 passes / 10 skips; HTML documentation passed. Slice 5 passed all 13
focused controls, including the four inherited local/mixed rejections.
Initial fixture and command setup failures are retained, not counted as passing
coverage. Details and exact commands are in the durable slice-4 ignored report
.codex/security-review/current-main-integration.md. Slice 4/5 build concurrency
was -j20, focused checks -j6 (final six controls -j4), mock checks -j20, docs
-j20. Elasticsearch, Kafka and MySQL were disabled in these targeted configure
lanes; service-backed coverage remains deferred to the final combined tip.

The umbrella source remains a historical reference, not the current integrated
candidate. Final parity checks must compare the assembled feature delta on
the current upstream base and explicitly retain upstream changes, rather than
equating raw trees across different upstream baselines.

Future full-reload work must address producer-local frontend consumer barriers
and graph ownership explicitly. The present stack will reject activation for
active local/mixed graphs; it does not pretend the legacy queue barrier covers
those workers. Lifting that restriction belongs to a later designed and tested
capability, not an unreviewed expansion during this rebase.
