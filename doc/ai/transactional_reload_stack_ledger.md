<!-- SPDX-License-Identifier: Apache-2.0 -->
# Transactional reload implementation and PR-stack ledger

Recorded: 2026-10-15. Status: the quality batch is atomically published through
slice 7, now draft PR #7690. All seven exact published candidates have fresh
passed security receipts. Hosted checks on the new runs remain pending; all
seven PRs remain draft, not merge-ready. Slice 8a is rebased and targeted-tested
but security review is pending; slice 8b parsing cleanup and fixtures are in
progress. Slices 8c and 9 still use old parents, with network validation held.
The helper-contract supplement has clear independent review of all eight files
but remains unpublished. Targeted evidence is not final-tip broad validation.
Umbrella: https://github.com/rsyslog/rsyslog/pull/7654 (draft).

## Method and merge contract

- Use durable sibling worktrees; never depend on /tmp for work or evidence.
- Before stopping any validation container, verify its exact ID and task
  worktree mount in a separate read-only check. Never infer ownership from
  it being the only running container or combine inspection and stop blindly.
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
- Read the builder's nested threat_model.revision when matching receipts.
  Expected source inventories exclude non-reviewable documentation. Use the
  builder's rename-aware changed-line count as authoritative for the review
  budget; raw rename-expanded diffs can create a false budget alarm.
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
| 8a | maxSessions capacity and session-owned regex state | eb2f4d101 (shared reconstruction checkpoint) | Rebased 51b209823de4f89af6e553e99652df23f81f5e6a passed epoll/poll 20/20 each, mock 1,596 passes/10 skips and docs; security pending; retain conservative maxListeners rejection |
| 8b | Sender ACLs and synchronous denied-session reclamation | eb2f4d101 (shared reconstruction checkpoint) | Source work is in progress; ACL support and denied-session closure are one functionality and must never be split |
| 8c | Independent maxListeners listener-table capacity | 2f52d3cfc | Local c6f8238773ae37270cc75c858a682659a40691ed has socket-free/native/docs/mock evidence only; network validation held |
| 9 | Simple named rate-limit policy lifecycle | 1a6c3daa8 | Implementation started on old 8c; recommended module-owned policy owner map removes controller-snapshot runtime borrows while preserving shared state and failed-resume ownership |
| 10 | Global/parser policy subset | 60dbf92ca | Move unrelated child-exit/scalar work here where practical; retain one-scalar rule |
| 11 | Remaining scalar profiles and coherent sampling | b2e01e773 | Fold parser helper/sampling fixes into earliest dependent scalar PR |
| 12 | Bounded prepared TCP add/remove/replace | 0d85d74e3 | Include poll capacity, readiness, startup serialization, diagnostics and docs together |

Pending order after slice 7: [8a](#slice-8a8b8c-and-9-ownership-plan) ->
[8b](#slice-8a8b8c-and-9-ownership-plan) ->
[8c](#slice-8a8b8c-and-9-ownership-plan) -> milestone 9 -> 10 -> 11 -> 12.
The 8a/8b/8c split does not renumber milestones 9-12.
Place the independent helper-contract supplement after the runtime slices and
before the later benchmark harness checkpoint; it is not a runtime dependency.

### Slice 8a/8b/8c and 9 ownership plan

Slice 8a groups maxSessions capacity changes with session-owned regex state. Its new
worktree is /home/rger/rsyslog-reload-stack-08a. Rebased checkpoint
51b209823de4f89af6e553e99652df23f81f5e6a on published slice 7 passed epoll
20/20, poll 20/20, mock distribution with 1,596 passes and 10 skips, and HTML
docs; security review is pending. Initial final checkpoint ec586b099 and
earlier 9b3f mock evidence remain historical. Slice 8b parsing cleanup and
fixtures are in progress; it introduces
sender ACLs together with synchronous denied-session reclamation. Never
publish ACL support separately from the closure/reclamation that makes it
correct.

Historical 2f52d3cfc changes maxListeners listener-table capacity independently;
it is not a prerequisite for slice 8a's maxSessions work. Slice 8a must retain
conservative maxListeners rejection. Track this umbrella feature explicitly
as the small planned 8c capacity checkpoint before 9, or an equivalent
coherent later split, rather than silently dropping it. Local slice 8c is now
c6f8238773ae37270cc75c858a682659a40691ed: socket-free/native controls,
HTML docs and mock distribution passed; network validation is held. This is
not network-tested or published clearance. Slice 8c still uses its old parent,
and network validation remains held for both slice 8c and the old-parent S9 work.

The slice-9 port requires explicit policy-owner handles. Static path review
confirmed that a historical unchanged named limiter can retain the old
registry while snapshot destruction waits; this has not been reproduced at
runtime. Treat it as an ordinary lifecycle/deadlock correctness constraint,
not an exploit claim. The future port must preserve unchanged shared bucket
state, give all consumers of the same policy one owner, and retain correct
ownership across failed resume and subsequent cleanup/retry. Source
implementation has started on the old slice-8c base. The recommended design
uses a module-owned policy-owner map to eliminate runtime borrowing from
controller snapshots. Historical 195/72 pin changes are intentionally not
copied if that ownership proof holds; this remains a design obligation, not
already-proven final-candidate behavior.

## Current evidence and remaining work

### Current published quality batch (slices 1-7)

This section records local work after the published correction batch below.
It does not replace historical published heads, validation results, receipts,
or incident provenance. Slices 1-7 are published atomically with exact leases
and the explicitly authorized intermediate-only container-validation override.
Upstream and fork main remain c62d84ad735404e23990806c53c882b0057b866c.
All seven have fresh passed receipts. Exact candidate receipts are recorded below;
older receipts remain historical and are stale for changed candidates. Other
changed candidates still require rebuilt inputs, fresh independent review and
matching receipts after their cascade stabilizes.

| Slice | Published quality checkpoint and focused evidence |
| --- | --- |
| 1 | 8bffa5860639278644459224a256ac1991982477 corrects the actual enum ABI mismatch at first introduction: restart-required=0, live-swap=1, new-sessions=2, drain-replace=3, with numeric unit assertions. New reload API lifecycle/ownership documentation and foundation wording were corrected. Focused controls 8/8 and HTML docs passed. |
| 2 | f595721f2e672e5e40e538c797618044fae07483 adds geometric wrapper growth and 4,097-item tests. Rebased locally as e33ad9a4d850bdc37c330dcd78ed011c7ea7e283 onto slice 1 at 8bffa5860639278644459224a256ac1991982477; focused controls 12/12 passed. |
| 3 | Frozen cascade head 22e0987287939402cf4bf5875d41443839aaa2d0 includes the production FNV helper and public cnfParse/CNF_SCANNER_ERROR portability fix. Modern Bison and Bison 2.3 focused lanes each passed 13/13. Old-skeleton generated objects used -DYYENABLE_NLS=0 -DYYLTYPE_IS_TRIVIAL=0 for Wundef compatibility; no macOS runtime claim. Earlier 18f285434 and tentative 6494e12fd remain superseded provenance. |
| 4 | Frozen locally at c1249b3730cd0854049fa05d56b0595e928e0d53; focused controls 12/12, HTML docs and conditional test registration passed; fresh security receipt passed. Earlier a56aedd623 is superseded provenance. |
| 5 | Final cascade head d6e35ef5a4410c6f33be889d3bae6aa419b07acb requires explicit CAP_REUSE and has a fresh passed security receipt. Earlier 67481934 passed 20/20 focused controls; keep that checkpoint evidence distinct. |
| 6 | Final cascade head b9f055dcd1f041f4acbc8db60a4583e6160a3bf7 has a fresh passed receipt, nine generation-guard controls and mock distribution: 1,630 total, 1,620 passes, 10 skips. Predecessor 11575ba8664e445ed48f9d9d12e923610cd4420b passed epoll 18/18, poll 19/19 and HTML docs. Earlier b02c97bb added the cold non-inline fix with x86 ASAN/native and production-object evidence, not original ARM64 reproduction; 41ec0507 remains historical provenance. |
| 7 | Published as draft PR #7690 at 99ae9fb8f65581a4cdc63c91ec5eeec0476e3dde, based on codex/reload-stack-06-live-policy. Epoll 22/22, poll 22/22, mock distribution (1,636 total, 1,626 passes, 10 skips) and HTML docs passed. Fresh discovery and lead review cover 21 files/875 changed lines with zero candidates and a passed receipt. Earlier ade6fe313 and its 1,591-pass/10-skip mock result remain historical. |

Fresh security receipts, all status `passed`, model revision 1:

- S1 8bffa5860639278644459224a256ac1991982477:
  `54b151c42b5dab4f138769eeae3cd815dec138f58a8275cc33b65ea94416a969`.
- S2 e33ad9a4d850bdc37c330dcd78ed011c7ea7e283:
  `51c1d765d8c13a6f8bda0383fc75c3f0ddab034e725d2cad5793bebbe3e47bd3`;
  expanded two-shard review and fresh validation retain the imdiag concern as
  non-blocking hardening, not a newly confirmed issue.
- S3 22e0987287939402cf4bf5875d41443839aaa2d0:
  `77614bd53aaa938dc321513445355f6ef1d890e9387e4e575100e53d760be957`;
  expanded two-shard review covers the corrected cascade.
- S4 c1249b3730cd0854049fa05d56b0595e928e0d53:
  `382609d06860fb11f476042f7ead1bf15aeef183e22ff67bd8adb3c826e607ba`.
- S5 d6e35ef5a4410c6f33be889d3bae6aa419b07acb:
  `3dea4d94ceec3eb0ad56a4c6a7e5d5fee46a2b883c309a3634297350038d7298`.
- S6 b9f055dcd1f041f4acbc8db60a4583e6160a3bf7:
  `eec0e94e70c78789cff106ac4ddac0c5ebfe9466082ce12e4e1aab260f6a3da8`.
- S7 99ae9fb8f65581a4cdc63c91ec5eeec0476e3dde:
  `3e03c08cd17144799008d85ee06c533df93d5168983f0c834bbe17db5653c835`;
  discovery plus fresh lead review cover 21 files/875 changed lines, zero candidates.

Slice 6's final generation guard protects ordinary exceptional-shutdown
cleanup; it is not a demonstrated vulnerability. The sustained handoff-timeout
availability limit is documented and must not be obscured by the passed review.

The independent helper-contract supplement is frozen at
da29c733f6c15bce5d3137b8e479d62c025e4b55, based on S7 ade6fe313. It ports
the historical helper return/cleanup work into eight existing C files. The
pinned build -j4/check -j2 passed three native units; CLI failure controls and
Valgrind allocation/append and assertion-failure controls passed with no
errors or leaks. The original control-command include-path failure is retained
separately from its corrected passing rerun. Independent review is clear for
all eight files; no PR exists yet. Placement remains supplementary, after
runtime work and before the benchmark harness;
this targeted handoff is not broad/analyzer clearance or publication.

The omdiscard allegation is not confirmed. The tested modern aliases are
invalid on a cold start, and legacy capture is unsupported; these observations
do not justify a feature change. No omdiscard capability expansion is part of
this quality batch.
The factual omdiscard correction was posted to the hosted AI thread as comment
4282810290. No review threads were resolved.

Hosted documentation-job failures reporting Codecov `Repository not found`
occurred after the documentation build succeeded. They are external upload
failures, not Sphinx build failures; retain that distinction when reporting CI.
All intermediate focused results above remain separate from the broad
container/analyzer gate deferred to the final assembled tip.

#### Actionable hosted compile failures

These failures are source/toolchain compatibility work, distinct from the
external Codecov upload failure above. Neither failed lane reached test
execution; neither is a sanitizer finding or an infrastructure failure.

- macOS repeatedly fails for PRs #7686/#7687 at `lexer.l:493` because
  `YYerror` is undeclared. Slice 3 now uses the public cnfParse interface and
  CNF_SCANNER_ERROR error path, with modern and Bison-2.3 focused validation
  recorded above; the local correction is not a macOS runtime pass. Evidence:
  [macOS build job](https://github.com/rsyslog/rsyslog/actions/runs/38742190218/job/116456733983).
- ARM64 ASAN fails to compile `shadowReloadResumePending` under GCC
  `-Werror=inline`. Slice 6 now uses an ordinary cold non-inline helper, with
  x86 ASAN/native and production-object evidence recorded above. The original
  ARM64 lane has not been reproduced locally. Evidence:
  [ARM64 ASAN build job](https://github.com/rsyslog/rsyslog/actions/runs/38742190218/job/116456733747).

### Historical hosted-feedback correction batch (previously published)

The five dependent branches were published atomically at 09450eb85 / 7c4b9cea8 /
ea52e6752 / 7d14e94c9 / b74f85476, with one functional commit per slice and
the correct immediate-parent bases. Freshly fetched upstream main remained
c62d84ad735404e23990806c53c882b0057b866c; fork main was fast-forwarded to the
same commit. Fresh independent Luna/high digest-bound reviews and receipts
are sealed for all five candidates. The serialized imdiag literal concern
was revalidated as non-blocking hardening in slice 2.

Previous published heads f21fdc83c / 2d05b707f / ba2f993cf / 33aeada71 /
659a3cbf8 and frozen pre-cascade fix checkpoints 4060068 / 66cbb3f / 2925e6 /
9ca7bea are historical provenance, not current heads. Publication used the
explicitly authorized intermediate-only container-validation override; the
final combined tip still requires the broad validation gate.

| Slice | Functional correction and focused evidence |
| --- | --- |
| 1 | Atomic scraped-gauge stores and pending lifetime; invalid/OOM policy status; guarded duration accounting; minimal HUP/TERM handlers, pending-TERM priority and main-context shutdown debugging; stronger legacy rotation and YAML accounting oracles. Focused lane 8/8, then final handler controls 2/2 passed. |
| 2 | Partial-send/reset handling, report provenance and duplicate-field controls; initial 6/6 and final post-cascade 8/8 passed (build -j4, checks -j2). |
| 3 | Portable working-directory handling, source identity and grammar corrections; initial 4/4 and final post-cascade 11/11 passed (build -j6, checks -j2). |
| 4 | OOM clone cleanup and report bounds; initial 13/13, alignment 2/2 and final post-cascade 16/16 passed (build -j4, checks -j2). |
| 5 | Final post-cascade focused TCP controls 20/20 passed (build -j4, checks -j2); actual assertion-enabled ASan/UBSan fuzz execution completed 1,000 iterations. |

Slice 6 is published as draft [#7689](https://github.com/rsyslog/rsyslog/pull/7689),
head b6e3c527e674e61f68070a6008caebb2d8f95f65, based on slice 5 b74f85476.
Its boundary is compatible existing-listener live policy and accept-time
new-session profiles; no listener addition/removal/replacement is enabled.
Final-head actual poll controls passed 19/19 (19.38s), configured with
`--disable-imtcp-epoll` and verified `mode: no`. Before the internal report
storage-shape/null-identity guard correction, ef1232893 passed epoll 18/18
(20.33s) and poll 19/19 (38.56s). Native controls cover the final guard;
there is no exact-final-head epoll claim.

Final mock distcheck passed: 1,631 total, 1,621 passes, 10 skips, zero failures
(69.34s). HTML documentation passed with -j20 at 89d03f12d; final doc/source
bytes are identical. The pinned Ubuntu 26.04 image recorded below was used
with `--no-pull`, GCC -g, build/check -j20; mock execution used
`CI_CHECK_CMD=distcheck` and `CI_MAKE_CHECK_OPT='-j20 TEST_RUN_TYPE=MOCK-OK'`.
Fresh Luna/high review used two shards covering all 32 expected files and
3,227 reviewable changed lines; receipt digest
`e9b4427d4c5d3406b0274352da19994335465e37c8b8d1dc788e10ebadc25acb`
is sealed passed. The explicitly approved intermediate-only publication
override was also used for slice 6. Detailed commands and evidence are in its
ignored `.codex/security-review/cascade-validation-summary.md`.

At that historical checkpoint, hosted checks and review-thread disposition
remained pending. The post-push
snapshot for the first five PRs has 75 unresolved non-outdated AI threads;
the earlier count of 94 is historical. Agents posted factual replies only and
resolved no threads reserved for human authority. Doxygen, ABI, performance
and style items still require review decisions. All six PRs remain draft and
are not merge-ready; mergeability alone does not change that status.
GitHub reports all six conflict-free against their declared bases. Hosted
jobs remain queued/in progress; cancelled duplicate runs must not be treated
as source failures. For example, slice-2 style run 38741228483 was cancelled
on the current head and superseded by pending run 38741230102 on that same
head. No cancelled-job rerun was requested while replacements were active.

At this published checkpoint, slice 7 was planned, not implemented:
identity-based retained-endpoint
reconciliation, mixed live/new-session profiles and unnamed rate-limit base
support. Keep addition/removal/replacement gated before Prepare until slice
12, and reuse the retirement retry already folded into slice 6 rather than
reintroducing historical ownership code.

An unrelated local-queue validation container was stopped by exact ID during
this session. The incident was reported to the user; no source was deleted
and no restart was attempted. The durable incident note is in slice 6's ignored
`.codex/security-review/stack06-validation-container-incident.md`. The ownership
check in the method contract above governs subsequent container stops.

### Earlier umbrella and stack evidence

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
[5 / #7687](https://github.com/rsyslog/rsyslog/pull/7687) ->
[6 / #7689](https://github.com/rsyslog/rsyslog/pull/7689) ->
[7 / #7690](https://github.com/rsyslog/rsyslog/pull/7690).
All seven are draft, assigned to rgerhards, milestone v8.2610 (ID 81), categories
configuration/enhancement. Each child targets the preceding feature branch.
Do not merge umbrella #7654 in parallel with this replacement stack.
All seven descriptions begin with merge conditions and link the complete 1-7
published order: parent first, retarget after its merge, verify the delta,
and pass required checks and review before merging the child.

Current atomically published heads and bases:

| Slice / PR | Current published head | Base |
| --- | --- | --- |
| 1 / #7683 | 8bffa5860639278644459224a256ac1991982477 | main / c62d84ad7 |
| 2 / #7684 | e33ad9a4d850bdc37c330dcd78ed011c7ea7e283 | #7683 / 8bffa5860 |
| 3 / #7685 | 22e0987287939402cf4bf5875d41443839aaa2d0 | #7684 / e33ad9a4d |
| 4 / #7686 | c1249b3730cd0854049fa05d56b0595e928e0d53 | #7685 / 22e098728 |
| 5 / #7687 | d6e35ef5a4410c6f33be889d3bae6aa419b07acb | #7686 / c1249b373 |
| 6 / #7689 | b9f055dcd1f041f4acbc8db60a4583e6160a3bf7 | #7687 / d6e35ef5a |
| 7 / #7690 | 99ae9fb8f65581a4cdc63c91ec5eeec0476e3dde | #7689 / b9f055dcd (codex/reload-stack-06-live-policy) |

Hosted new runs and hosted review-thread disposition remain pending. Fresh
local receipts and conflict-free publication do not establish merge readiness
or replace the final combined-tip broad container gate.

### Historical opening and integration evidence

| Slice / PR | Published opening head / base | Functional checks before latest main integration | Review and other evidence |
| --- | --- | --- | --- |
| 1 / #7683 | 557693312 / main | 8/8 policy/unit tests (104.58s); corrected checkpoint intervals 0/1/5 all pass (7.22s) | Independent Luna/high discovery and lead covered all 34 expected paths; earlier analyzer, no-systemd, mock distcheck, HTML and extended docs distribution passed |
| 2 / #7684 | 6b7e5ddd7 / codex/reload-stack-01-foundation | 7/7 report/graph/frontend checks (100.24s); both field-omission negative controls fail, restored control passes | Independent discovery, lead and validation completed; diagnostic concern classified non-blocking hardening; mock distribution and HTML passed |
| 3 / #7685 | 36b6722fa / codex/reload-stack-02-reports | 10/10 candidate/graph/frontend/error checks (100.09s) | Two Luna/high shards covered all 21 expected paths plus parser/lexer context; mock distribution and HTML passed |
| 4 / #7686 | 097f45f8f / codex/reload-stack-03-candidates | 12/12 clone/barrier/worker/activation checks (100.40s) | Two Luna/high shards covered all 36 expected paths; HTML passed; specialist sanitizer/distribution evidence remains distinct |
| 5 / #7687 | 99ded8113 / codex/reload-stack-04-ruleset-activation | 9/9 detached-candidate/frontend/TCP checks (55.58s) | Two Luna/high shards covered all 17 expected paths; HTML passed; permitted-peer allocation cleanup folded here, static error-path audit, no forced-OOM runtime claim |

These opening heads preserve provenance and differ from the current published
heads above; refreshed evidence is recorded above. Each slice contains one functional commit
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
At that published-head checkpoint, all five security packages had complete,
unique reviewed-file coverage and passed receipts against freshly rebuilt
clean inventories. Those historical receipts are superseded by the fresh
correction-batch reviews recorded above. Hosted checks and review remain
separate merge requirements, not
implied by mergeability.

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

Historical slice-6 construction boundary: bounded compatible existing-listener live
policy and accept-time profiles. First-introduction fixes include poll capacity
and failed-resume ownership/automatic retry before candidate disposal. It must
not enable listener addition/removal/replacement. Construction used
the integrated parent; module/input-only changes must also reject active
local/mixed graphs before any module preparation or publication.

Historical current-main integration checkpoint: slices 1-3 were published on
c62d84ad7 at f21fdc83c / 2d05b707f / ba2f993cf. Their refreshed focused
lanes passed 13/13 (71.38s), 9/9 (63.89s) and 12/12 (64.00s), including
RainerScript/YAML local-queue configuration controls and all three checkpoint
intervals in slice 1. Fresh independent security reviews cover those exact
deltas; receipts are sealed against the rebuilt clean inventories.

Historical pre-quality-batch published heads (superseded, retained for provenance):

| PR | Historical published head | Historical verified base |
| --- | --- | --- |
| #7683 | 09450eb85f50257a51b137790dabdbe8f667c64b | main / c62d84ad7 |
| #7684 | 7c4b9cea8fed73772c4e9cc0da43f8e135f1676e | #7683 / 09450eb85 |
| #7685 | ea52e6752a359b744b94fe918692e7f7e5c788a2 | #7684 / 7c4b9cea8 |
| #7686 | 7d14e94c9e35e17146aa00010e0293172d99bc81 | #7685 / ea52e6752 |
| #7687 | b74f85476b972ad89ed18a10949b7c4a30609094 | #7686 / 7d14e94c9 |
| #7689 | b6e3c527e674e61f68070a6008caebb2d8f95f65 | #7687 / b74f85476 |

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
