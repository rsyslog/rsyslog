<!-- SPDX-License-Identifier: Apache-2.0 -->
# Transactional reload implementation and PR-stack ledger

Recorded: 2026-10-17. Latest publication is twenty-two draft stack PRs through
slice 14b. Ordinary introducing fixes are folded into their owning slices and
children are recascaded with backup refs and own-patch range-diffs preserved.
The full nine-milestone implementation plan continues; security work remains
explicitly deferred. No merge readiness or hardware performance acceptance is
claimed. Historical bounded-tip broad and analyzer passes remain separate from
the latest assembled candidates; older passing receipts do not validate new heads.

## Continuing implementation checkpoint (2026-10-17)

The published22-link stack remains open and draft. The latest local correction
cascade ends at14b fbc8471312d149545fdf983b53b34be027510d41 and is not yet
republished. Owning02 is5f218c01346f9e29e8a09cb198c1faa6914718c4; owning03
is f9e5e6c8044488dda824c188ece9727870e6cd8b. The03 fix treats NULL grammar
include reductions as no program fragment, while preserving real empty syntax
objects. Its length-aware native clone fixture repair is folded into03;
portable explicit struct initialization is folded into02/03. Descendants were
recascaded with backups and range-diffs;06 adapts the inherited test to its
source-catalog output API. Actual CentOS7 GCC4.8.5 report/graph/candidate units
passed3/3; the hosted-equivalent Debian i386 candidate unit passed, including
the previously failing fingerprint assertion. Original failures remain saved.

Published14b e9845c2c7005ca15c2ada4c11c22ac3be3e7bee2 passed its full
analyzer with zero reports. Its broad runtime result was1851 total,1779 passed,
70 skipped,2 failed,zero errors. The failures were an imtcp max-listeners test
startup timeout before HUP and the imczmq startup-failure harness deadline.
Both passed unchanged isolated reruns2/2; causes remain unresolved. This is
not a clean broad-suite pass. Image Ubuntu26.04 ID
sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09,
run-ci.sh build/check60, analyzer20, default relevance retained all service
families. Exact logs and failure triage are in the dedicated
rsyslog-reload-m4-checkpoint-validation .ordinary-validation directory.

Local14c b49f49e5dc741a7987c4a20dc07fc202082a4213 implements default-program
activation with separate synthetic identity, ordered fragment aggregation,
empty transitions, named/default atomic publication and explicit legacy-routing
and unnamed-action ownership restrictions. Its corrected-parent focused suite
passed8/8 with no skips, including real RainerScript includes and hybrid YAML
(native named bodies plus included RainerScript default statements). Native
ASan/UBSan with leak checking2/2 and Clang21 O0 native2/2 passed; the fence
fixture linked under O0 TSAN. Exact formatting passed107 C/H files. Clean HTML,
YAML docs and mock distribution evidence carries forward across unchanged doc
and distribution inputs. Independent ordinary correctness review is clear for
diff SHA256 c1bbe689799e63c45fef66ceb6efcee4b164821317fa0184155aef0979b89d57.
Native YAML top-level statements remain a future frontend proposal.

Local14d e95f42e2b7c36609443f16f44377120a7f6c1e9e is reparented onto14c;
its own patch is unchanged from da5c4383a5d68dd4f06903144f57d8ce051b28a6.
It adds nonblocking inspection of every worker slot after completion Wait,
including exited-worker retained responsibility, and denies starts while
QUIESCE is active. Contention, invalid generations and retained work remain
distinct outcomes. Normal targeted tests passed in isolated selections, the
fallback-atomic native fixture passed, and TSAN native plus real lease checks
passed2/2. Initial startup failures remain preserved. This does not prove
backend emptiness or enable ruleset reclamation. A fresh assembled checkpoint
at e95f42e2b7c36609443f16f44377120a7f6c1e9e is running sequential Ubuntu
runtime60 and clean analyzer20 in rsyslog-reload-m4-default-checkpoint.
The analyzer passed with zero reports. The broad runtime result was1857 total,
1786 passed,70 skipped,one failure,zero errors: imptcp-bind-failure.sh terminated
its best-effort case before healthy-listener readiness and recorded bind EINTR.
An unchanged isolated rerun passed1/1 with both frontends enabled; the original
cause remains unresolved, and this is not a clean broad-suite pass. Runtime and
test-suite logs retain collected failure output; distclean removed individual
per-test/fixture files before triage, and that artifact limitation is recorded.

The actual CentOS7 GCC4.8.5 all-check_PROGRAMS compile found concrete later
portability failures: the08b address-getter pointer type, unavailable C11 atomic
header in a06 test, nested aggregate fixture initializers and a13b cancellation
callback return type. The reviewed aggregate repairs preserve all test oracles
and atomic orders. All55 native programs compiled/linked and11 affected units
passed on CentOS7. Exact clang-format18 normalization is whitespace only;
formatted patch SHA256 is
2bf82c1d7f4143150f43115a398774e07f0a46911d9a41f3b87babc6d0a0ee6a.
The additive local portability commit is5d806ed88a820b3125a5d5b3baf971466e213f10;
its two affected threaded fixtures also passed Clang21 O0 TSAN2/2 in the pinned
Ubuntu image with CI ignorelist/suppressions and seccomp unconfined. Original
failures, owner-hunk inventory and source/image evidence remain preserved.

Automatic approval review rejected the batch owning-slice amendments and
strict descendant rebases before execution, citing insufficient explicit
permission for that history rewrite. No stack history changed. A specific user
choice is pending. Prepared owner patches are in
/tmp/reload-centos-owner-fold-20261017; publication remains provisional. Work
continues additively on a new integration branch, preserving every stack ref.

Slice15 is locally committed at863df16091cd514dcacbb57cc67862391107e313 on its
preserved historical parent da5c4383; no rebase or push. Its optional append-only
module tail and separate reader-owner array avoid normal module commit/retire.
Real imtcp pins cover draining endpoints and joined teardown invalidation.
Ordinary review identified and verified a correction: reader-only requests now
reject all global/profile changes after classification and before preparation
or publication. Real parsed regression controls preserve baseline, generation
and parser settings, and retain ordinary global/body-only positive paths.
Original focused14/14, subsequent ABI3/3 and corrected-scope5/5 passed. Original
ASan/UBSan3/3 and TSAN3/3, followed by affected-manager refreshes1/1 each,
passed; final ordinary review is clear for diff7f8d5ef4374c281eb9813c0c3f69a88c5fc8af172206fbb7f722af66a76d790f.

Additive integration14921d8752f1b2b2ed4685372e29c7b3261fb509 combines current
14d, the reviewed portability fixes and15. Every added/deleted15 feature line
matches the reviewed source; only inherited context and additive test-block
placement differ. Combined focused native/runtime tests passed15/15, no skips,
including default programs, legacy routing controls, ordinary parser-LF reload,
queue slot/lease checks and actual reader ownership. Formatting passed110 C/H
files. This is targeted container evidence, not a new broad/analyzer pass.

Separate16 imdiag reader coverage is under implementation: protect injection
through limiter cleanup while copied status commands stay available, classify
helper ownership conservatively, and prove actual worker retirement before gate
teardown. Separate17 adds owned parser-assignment profiles without enabling
activation or changing existing object visitors. Both remain in progress.
Input reader coverage does not establish asynchronous output/private parser
coverage. General removal, parser activation consumers and later milestones
remain open. Security work remains explicitly deferred.

## Published continuation checkpoint (2026-10-16)

### Latest deterministic validation and continuing work

The bounded assembled tip 96411a585f2816ffb99e50cd6430fb6800faa31d
passed the full clean Clang analyzer (zero reports) and the change-gated
Ubuntu 26.04 run-ci.sh suite: 1839 total, 1769 passed, 70 skipped, zero failures,
errors or unexpected passes. Runtime build/check60, analyzer20, same pinned
image/default configure options and documented analyzer exclusions. The passing
broad evidence was recorded with devtools/record-container-validation.py;
container_validation.json in the benchmark integration worktree binds source,
image, full suite log and explicit coverage limits. This is deterministic local
container evidence, not security, performance or PR merge readiness.
Focused ASan/UBSan30/30 and TSAN6/6 also passed on that exact head.

Call-graph draft #7708 is published at a5ab93e601a141e88852a221bdd4bc8e92e9aec3,
parent 38477ae248ade77c9e8617105d768e3a6c1acb3d. That exact new candidate
passed full clean analyzer, both compile-portability lanes, full-image mock
distcheck (1706 total,1696 pass,10 skip,zero failure), focused ASan/UBSan9/9
and TSAN9/9, plus independent ordinary review. The earlier parent's broad pass
is not a broad pass for this patch. New GNU sed -i fixture calls were caught;
the owning13b script is corrected locally at016a296a6, with real RS/YAML2/2,
strict POSIX-sed2/2 and failure-preservation controls passed. It is not yet
republished; the old source and logs remain preserved.

Local assembled checkpoint14b is e9845c2c7005ca15c2ada4c11c22ac3be3e7bee2,
parent14a1b22db1124add5a3b4c100ad3b6df27be2944c42 above corrected13b
61f579c77fee98554bdc646259c0e0b70ce4a14b. Both new slices and all recascaded parents are now published as drafts
(#7709 and #7710); the complete22-link order is in every PR description. Ordinary assembled review is clear; the full analyzer passed with zero reports on this exact head, and the broad
runtime gate is running in rsyslog-reload-m4-checkpoint-validation. Exact final checkpoint focused
normal13/13 and TSAN11/11 passed (CI queue.c ignorelist remains a coverage
limit; impstats disabled per TSAN CI, seccomp unconfined, CPU4/build4/check2).

Additional queue-less named rulesets are implemented in14a: immutable name
blocks, private two-pass shells/program binding and startup namespace publication
before activation. Both frontend traffic scenarios passed, including forward
references, repeated additions, indirect lookup visibility, old-call latching
and rollback for unsupported changes. Seven focused tests, four private
ASan/UBSan tests, clean HTML docs and MOCK distribution checks passed. Final
portability corrections were folded into14a: validated report-row address
calculation for Clang cast-alignment checks, and an explicit abort sentinel for
a native direct-plan fixture helper never reached by its tests (O0 linking must
not depend on dead-code elimination). Original failures, backups and exact
checks are retained. The two Tier2 compile lanes passed at3dd85dde, whose
production source is identical to the final checkpoint; only that test sentinel
differs. Ordinary review explicitly carried forward across the sentinel change.

The14b opt-in completion barrier uses real preceding-batch completion before
worker acknowledgment and includes DA transfer workers and child consumers.
Its own normal10/10, expanded DA2/2 and ASan/UBSan10/10 passed. The first
sanitizer build hit the existing generic module callback function-type check
before queue startup; that original log is retained, and the passing run kept
ASan/UBSan with only the function-type check disabled. Completion is not
positive emptiness proof: departed-worker responsibility, backend state and
reader ownership still need proof before removal or reclamation is enabled.

The next14c implementation activates the default program, using a distinct
synthetic default-program identity so a legal named default ruleset stays
separate. It aggregates source fragments before compilation and invalidates
startup capture for legacy current/default routing changes. Focused controls
will cover source order, STOP, empty transitions, named/default atomic changes
and action ownership. Source changes and tests remain under development.
Parallel14d work adds explicit nonblocking inspection of every worker slot,
including exited workers with retained leases. It preserves the existing Wait
contract and distinguishes contention, invalidated generations and retained
work, without claiming queue emptiness or enabling removal. Both worktrees
preserve the frozen assembled checkpoint;14d will be reparented after14c.

The14a work discovered an older admission defect. Original slice4 was
reproduced accepting a named input-bound ruleset queue.type change as activated
generation2 while only its program could be replaced. The owning-slice fix
adds distinct normalized ruleset metadata. Both frontend regressions now reject
queue/parser attribute changes without advancing generation while supported
body changes activate. Owning-slice ASan13/13, mock distribution checks,
targeted analyzer and independent ordinary review passed. Children were
recascaded with backup refs; test source-catalog API adaptation was folded into
owning6. The first assembled metadata analyzer passed, but its broad run used
the pre-adaptation source and failed to compile that unit test; this failed
checkpoint is preserved, not counted as a passing gate.

The corrected assembled41a156e1 checkpoint passed eight selected sanitizer
tests before two exact report-count assertions failed. The eight integrations
aborted by fail-fast were run separately and all passed. A complete changed-test
audit found26 stale exact assertions across five shell files; each correction
changes only unchanged+1 for the explicit metadata row. All generation/result/
added/removed/modified/invalid/capability predicates are preserved. Corrections
were folded into their true introducing slices and all descendants recascaded,
with context-only range-diff exceptions recorded rather than misreported as
literal equality. Final13b61f579c77 passed the corrected two frontend tests
under ASan/UBSan2/2. Full original failure and separate passing logs remain
preserved. The complete nine-milestone implementation continues; security work
remains deferred and no older green receipt is presented as current acceptance.

The earlier hosted snapshot used full statusCheckRollup for all20 then-existing
drafts. New heads and the two new PRs need a fresh hosted snapshot.
Current failures included seven external Codecov Repository-not-found uploads
and one Ubuntu20 pmsnare-ccoff-udp missing-output test failure on unchanged
foundation8a239d6. Its cause remains unresolved; only that failed job is being
retried on unchanged source with original logs retained. Cancelled runs are
counted separately. Reserved review threads remain untouched.


The introducing fixes for the analyzer allocation representation (slice 2),
old-GCC initialization and portable fixture edits (slice 4), accepted-session
publication/cleanup (slice 8b), and prepared-backend busy stop/session cleanup
(slice 12a) are folded locally and cascaded through coherent slice 12b.
All original heads are preserved by dated backup refs. The cascade's own-patch
range-diffs are unchanged except the documented merge resolutions: slice 6
retains both portable guard initialization and input pause accounting; slice
12a deduplicates ADD-error propagation and the daemon-link helper already added
by slice 8b; slice 12b retains both native test registrations and the atomic
stop contract. Exact old/new heads: `.ordinary-validation/cascade-20261016.json`.
These heads are now published. The older published-head table farther below is
a historical checkpoint and must not be mistaken for this current state.

### Current draft PR order and exact heads

| Slice | PR | Published head |
| --- | --- | --- |
| 1 | #7683 | 8a239d6b92c95825cec06fe921e066f768ec86a3 |
| 2 | #7684 | bb52fbe414586fa2ca46c63da8c0ce18cb07c146 |
| 3 | #7685 | 58744ade952662666e96ab06c41fa45988d53fa9 |
| 4 | #7686 | 68a30697c0332744815b2a2fd3cdefe09c8bae1b |
| 5 | #7687 | f66ff489a3f28dca90fc1ecff35bf5087d8a3b40 |
| 6 | #7689 | a5b650222c01866571ce28e55d25ae946f0dcdd5 |
| 6a | #7691 | 37e008e0fb5f12bd2983aa4d48189f841d9f1d88 |
| 7 | #7690 | defde6a79d8a612d91dca4eb873828f7f535f1db |
| 8a | #7692 | c14352b4e6c159b9d04d3620af5c8953ff8a8bcf |
| 8b | #7693 | cf5eb255358a2660d3b2248a8aca8180a76b40ff |
| 8c | #7694 | b5feee26e0f2dcb4d025aa23be067fd8ecdfb749 |
| 9 | #7695 | ac5031a4af1e7b55fbbb75b8a8950f5db9b0bce6 |
| 10 | #7701 | 47b6789cfad59103d8c0f06fddfa7ae243f7b4c9 |
| 11 | #7702 | 018d85b6aea0bf90d4a3e4491a0d6e73d2542a5e |
| 12a | #7703 | f946563eda731e01efadc2197c5ae49e1953ae71 |
| 12b | #7704 | 00638ad63e78b21e5fa21370ed793cb83db26ded |
| helpers | #7705 | 4f4978084010f617590754eafb85552ebe9dcf10 |
| benchmark | #7706 | 6b56f3bccc75c676c1ee2c259dbb9cf350ad593f |
| 13a | #7707 | bd67ea4d1dbeff629e3bef98c8375c14eda13dfe |
| 13b | #7708 | 61f579c77fee98554bdc646259c0e0b70ce4a14b |
| 14a | #7709 | 1b22db1124add5a3b4c100ad3b6df27be2944c42 |
| 14b | #7710 | e9845c2c7005ca15c2ada4c11c22ac3be3e7bee2 |

Slices1-11 continue using upstream source branches;12a and children use fork
PR heads. Matching upstream parent-base twins now exist through14a so every
child targets its exact immediate predecessor. The twins must stay synchronized
in future cascades. Fresh upstream main remained332da35217b3ead9fdf95436d6b4790657f9015b.
Exact-lease atomic pushes changed18 upstream refs and8 fork refs. No PR was
merged and no reserved thread was resolved. Both new draft PRs are assigned to
rgerhards, use milestone v8.2610 and configuration/core enhancement labels.
All22 descriptions were updated through the GitHub connector with current heads,
parents, merge order and validation limits. Publication manifest, original leases,
push logs and exact descriptions are under
/tmp/reload-publication-20261017/ and copied into the canonical evidence directory.
Intermediate publication used the previously expressly authorized
SKIP_CONTAINER_VALIDATION=1 override; no final readiness claim is implied.

An earlier fork publication attempt used a local unpublished 12b head as its
expected lease and was atomically rejected without changing any ref. A fresh
remote-ref check established the new refs were absent; the corrected exact-
lease atomic publication then passed. This was a normal Git lease rejection,
not a security or approval block. Intermediate publication used the previously
expressly authorized SKIP_CONTAINER_VALIDATION=1 override.

The full analyzer first reported one native-test initialization model warning;
slice 11 now explicitly queries/checks its required object interface and fails
setup cleanly if the postcondition is absent. The actual unit and targeted
Clang scan pass; no production crash is inferred. This correction is folded at
408f8455c and all children are recascaded unchanged. Current assembled tip is
96411a585f2816ffb99e50cd6430fb6800faa31d; its full analyzer rerun and
change-gated runtime check run in separate clean worktrees. Runtime uses the
pinned image defaults and machine build/check60; all relevance gates retained
service families because shared runtime/testbench changes are relevant. The
image's default disabled service-test options remain unchanged. The analyzer
uses the skill's documented service/module exclusions and build20. Earlier
63f25c567 passed both compile-portability lanes, mock distcheck (1691 pass,
10 skips, zero failures) and clean strict docs; only the native setup differs.

Post-cascade actual poll compilation exposed epoll-only native callback/static
interface definitions outside their backend guard. That test omission is folded
into slice 8b, then cleanly recascaded. Combined epoll on 044cf2a6e passed 30/30;
poll on 94fa76b75 passed 29 with one expected epoll-only skip (77), zero failures.
Their only source-tree difference is the three-line backend guard, so the epoll
compiled source is identical. Original failed poll output is preserved.

The assembled helper and benchmark tip is
63f25c567392dca3ac2072882e1e804210c0bd67, with helper parent
b065cec46830e61ff88245b154e714ea00c379c2 and lifecycle parent
94fa76b7582d6e7759deb5fe90e8369037f0c70c. Original helper/benchmark
checkpoints are untouched. Independent review cleared the benchmark path
contract correction; its software selftests pass. Whitespace, exact C format
and Python style checks pass. Shellcheck finds 57 warnings in existing portions
of changed shell files, zero in changed hunks; those warnings are recorded,
not described as a whole-file pass. Static analyzer, mock distcheck and compile
portability are running in separate clean validation worktrees. No hardware
performance acceptance has passed.

Milestone-4 representation-only slice 13a is locally committed at
5a4e0c17509421ad4405e428ac74faf2aab2e61e on that assembled tip.
Independent ordinary review, seven focused tests, explicit missing-target
rejection and distribution inventory passed. A shared private dispatch unit
checks root replacement/empty roots, async selection and error propagation;
it does not exercise a complete live reload. Slice 13b is implementing existing-
membership static/indirect call graphs with complete execution fences. Membership
reclamation and later milestones remain open. Slice 13a is now published as draft #7707; slice 13b remains in progress.

Slice 12b implements bounded numeric fixed plain-TCP listener additions,
different-endpoint replacement and draining removal, including rollback and
backend-owner health propagation. Its initial focused epoll/poll, compatibility,
distribution and strict docs checks passed; combined post-cascade backend checks
are in progress. This does not implement arbitrary listener/TLS reload.
The helper and benchmark source checkpoints are being assembled onto this tip.
The benchmark artifact-path correction passed independent ordinary review and
software controls; no hardware performance campaign has passed.

A new milestone-4 preparatory slice replaces static-call cached statement roots
with stable ruleset references while retaining the existing conservative
activation/membership restrictions. Complete membership needs explicit coverage
for persisted-message name resolution, parser/module routing bindings, input
batches, rate-limiter history and queue-held batches; session counts and apparent
queue emptiness alone are insufficient. The remaining milestones remain open.

The platform cybersecurity content classifier interrupted one ordinary runtime
repair worker after corrected-code ASan tests passed. The saved work was
independently reviewed and preserved. Its proposed original-code negative
control did not run; no such result or security clearance is claimed. This UI
restriction is separate from execution approval and the Git push validation hook.

## Full implementation plan and continuation contract

The maintainer requested continuation of the full nine-milestone plan recovered
from the original HUP session (2026-10-07 and reaffirmed 2026-10-15), not only
completion of the current PR stack. The target is transactional configuration
reload without a daemon restart or unnecessary disconnection of established
TCP sessions. Failed candidates leave the active configuration intact. A narrow,
explicit restart-only list remains permissible; unsupported changes cannot be
silently accepted.

| Milestone | Required functional outcome | Current work |
| --- | --- | --- |
| 1 | Close the bounded foundation with real epoll/poll, cleanup, distribution and focused regression coverage | Stack through 12a is published; a new ordinary busy-stop correction is being folded into 12a before its child depends on it |
| 2 | Add fixed plain-TCP listeners privately and publish only after commit; setup failures preserve active listeners/sessions | Historical umbrella implementation exists; reconstructing the coherent slice-12b candidate on current 12a |
| 3 | Stop new accepts on removal, drain sessions, handle replacement conflicts and eventually reclaim owners | Included with 12b; original mixed WIP is preserved for comparison |
| 4 | Add/remove rulesets and update complete supported call/dependency graphs with safe old-message/session ownership | Planned after the bounded foundation is assembled and its functional checks pass |
| 5 | Reload templates, parsers and stateless processing definitions coherently with rulesets | Planned; immutable generation ownership and old-consumer lifetime required |
| 6 | Prepare, publish and retire actions, outputs and their files/connections/workers with rollback | Planned; no blanket module capability acceptance |
| 7 | Define supported queue drains/migrations and preserve delivery/persistent state across transitions | Planned; retain explicit rejection of unsupported changes |
| 8 | Extend lifecycle contracts beyond plain-TCP, cover TLS and coherent compatible global transactions, classify every configuration category | Planned; restart-only categories must be explicit |
| 9 | Prove mixed changes compose, repeated traffic reloads, rollback/shutdown, resource cleanup, performance and release validation/documentation | Final acceptance remains open; deferred review is not a pass |

Finish the helper-contract and activation-benchmark source checkpoints after
12b, then validate the combined bounded tip. Keep all intermediate PRs draft,
individually coherent after their predecessor, and linked in strict merge order.
Fold ordinary corrections into the slice introducing the behavior and rebase
children without losing their own functional delta. Do not merge any PR or
resolve human-reserved threads without the maintainer's instruction.

Security work remains explicitly deferred. Continue implementation, ordinary
correctness review and functional validation without starting security scans or
switching to Daybreak. Final release/readiness claims retain the outstanding
review and performance requirements.

Fresh host preflight on 2026-10-16 reconfirmed that the task-local perf tool
reports all five required hardware PMU events as `<not supported>` despite
exit status zero: cycles, instructions, branches, branch misses and cache misses.
No host settings were changed. A benchmark-host question is pending with the
maintainer while implementation and deterministic local validation continue.
Software counters or harness self-tests cannot satisfy this hardware evidence.

### 2026-10-16 ordinary integration checkpoint

The user requested continued implementation and focused validation, with further
security work deferred. Existing blocked findings and merge requirements remain
in force; no security clearance is inferred from this deferral.

The property-filter startup correction is folded into local slice 2 at
af58d12f28f1de5434f59bb8f0e4f3b292c927ce. Independent ordinary review found no
issue; the deterministic regression failed before the fix and passed afterward.
The native unit and both affected omruleset tests passed under Clang ASan/UBSan.
The regex preparation-bound correction is folded into slice 8a, with independent
ordinary review, native boundary red/green, both frontend integrations, and
header distribution checks passed. These are targeted results, not a broad gate.

The first cascade through 8a was conflict-free. A second ordinary integration
folded then-only property-filter semantics into slices 4 and 6 and OFF-mode
retirement accounting into slice 10, then recascaded through slice 11. Backup
refs preserve every preceding head. Manual resolutions retained both endpoint
and poison-slot test controls, accurate comments, and both regex/policy-owner
headers; other own-patch range-diffs are unchanged. A subsequent packaging-only
upstream refresh was conflict-free, with all fourteen own patches unchanged.
Current published heads:

| Slice | Published head after ordinary correction cascade |
| --- | --- |
| 1 | 8a239d6b92c95825cec06fe921e066f768ec86a3 |
| 2 | 6766945dd9dbada49303c5843d26ca84c47ccfbd |
| 3 | e09406c53ef0fed49216a24bdff8eae6fa2b9107 |
| 4 | 6f2b5a53a93aa42c62ee303748ab99d676531d73 |
| 5 | 40e90fe913039376915937beeca4945c04dd287b |
| 6 | 97424f55a2c38df1f0e74ac31ceb50efbcb75f9e |
| 6a | 110a9d8ba38c18bc181d6e022991f5aec9f435a4 |
| 7 | 3fcc358dfcafad9ab9ed8cba9aa6ffd206109dc5 |
| 8a | 529c02f6806cbe2527b2108845b9c7094a287966 |
| 8b | c88bff9e123b859aca98c6c888c1879595b7ddae |
| 8c | cf90fc62317df225fef016762366d19f5e6fc3b5 |
| 9 | e8670e2babd0fe6c446b27a7bcaa2483c1e2a6dc |
| 10 | 961e9e04e1bcff6120321e287065ef2b2f49ea22 |
| 11 | 0b930aad80c9ac127f7dd1d4ec53e34b8b922f67 |

The first-cascade 8a snapshot 2f35bc02 passed twenty focused cases per actual
epoll/poll backend. That result is historical and does not validate subsequent
edits. A forty-five-case selection passed on combined tip e9d5aca607306fab866e750292b6b83f36163d6c
under actual epoll (45/45, 32.396s) and poll (45/45, 32.050s), with zero skips,
failures or errors. After the upstream refresh, only three openSUSE packaging
files differ; all runtime/test bytes are identical. These are runtime-tree-
equivalent results, not exact-new-HEAD executions. New evidence uses ordinary-
validation paths. The fourteen branches are published with exact leases and
the previously approved intermediate-only validation override. Earlier receipts
do not transfer to rebased candidates; review is deferred, not waived.

The slice-10 mode-switch/retirement accounting edge case is corrected:
an ON-to-OFF activation combined with a supported input update can retain
pending retirement after resume fails. A subsequent blocked OFF HUP must not
increment the ON-mode rejection counter. The global-only REUSE case does not
have this module-resume failure path. Its production selector has a failing
OFF-mode negative control and passing restored control; two focused native
tests and independent ordinary review passed before the correction was folded.

The slice-4/slice-6 changes align reload walkers and materialization with
then-only property-filter semantics while preserving real IF/priority-filter
else branches. Unlike the pre-slice-4 constructor involved in the slice-2
startup defect, slice 4 initializes its unused slot to NULL. Poison-slot native
red/green evidence is synthetic and is not a reproduced slice-4 parser crash.
The two new ruleset visitor changes have source review, not a direct visitor
poison test. The strengthened YAML condition-only reload/output test passed.

An ordinary architecture review of incomplete slice 12 recommends a coherent
prepared-backend/readiness checkpoint before complete listener topology wiring.
The current dirty mixed implementation is not a merge candidate: its classifier
and preparer do not yet implement the same topology contract. Native partial
worker-startup and RequestStop oracles remain required. The existing source and
evidence are preserved; guarded full lifecycle wiring remains untouched.

A separate slice-12a worktree now isolates the prepared-backend foundation:
exactly four appended API methods (ABI 40), worker readiness/cooperative stop,
minimal private prepared socket setup and a real production-backend native test.
It has no imtcp configuration caller and adds no advertised listener topology
support. Full pinned-image module/dependency builds, native tests and six TCP/
reload smokes passed under actual epoll and poll (build 4, check 2, 4 CPUs).
Independent ordinary review and distribution checks passed. The source is now
committed and published as [PR #7703](https://github.com/rsyslog/rsyslog/pull/7703)
at c0379e94abf3ae4772f0342e8f2fb0745bcffb6d on the slice-11 parent above.
Takeover verification confirmed all eight source paths byte-identical to the
previously tested archive; fresh poll native controls passed and mock distcheck
reported 1,162 total / 1,156 pass / 6 skip / zero fail or error. Fresh formatting
and whitespace checks passed. Full logs and command/image details remain in
slice 12a's `.ordinary-validation/README.md`. This is targeted-container-tested
intermediate work, not full PR-ready validation; security work remains deferred.
The original mixed WIP is preserved. The takeover session owns continuation.


Published order now continues from [9/#7695](https://github.com/rsyslog/rsyslog/pull/7695)
to [10/#7701](https://github.com/rsyslog/rsyslog/pull/7701) to
[11/#7702](https://github.com/rsyslog/rsyslog/pull/7702), then
[12a/#7703](https://github.com/rsyslog/rsyslog/pull/7703) backend foundation and
planned 12b complete listener lifecycle. Slice 12a targets slice 11, is assigned
to rgerhards, uses milestone v8.2610 and enhancement/configuration labels, and
remains draft. All fourteen parent descriptions now link 12a in the merge order.
Upstream main and all published feature branches use the verified 332da352 base.
Fork main synchronization was previously guard-blocked and has not been retried.
The prior session's explanation of a UI cybersecurity content restriction as a
specific lifecycle-edit execution denial is not established by that banner;
the continuing session does not infer a source prohibition from it.

Previous published checkpoint: the quality batch was atomically published through
slice 7, draft PR #7690. All seven exact published candidates had passed
security receipts at publication. All seven PRs remain draft, not merge-ready.
Current local slice/review progress is recorded in the new batch section below;
network validation for the old-parent later slices remains held.
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
| 9 | Simple named rate-limit policy lifecycle | 1a6c3daa8 | Current port is local on published 8c; module-owned policy handles retain shared configuration, listener-local buckets and failed-resume ownership |
| 10 | Global/parser policy subset | 60dbf92ca | Move unrelated child-exit/scalar work here where practical; retain one-scalar rule |
| 11 | Remaining scalar profiles and coherent sampling | b2e01e773 | Fold parser helper/sampling fixes into earliest dependent scalar PR |
| 12 | Bounded prepared TCP add/remove/replace | 0d85d74e3 | Include poll capacity, readiness, startup serialization, diagnostics and docs together |

Pending order after slice 7: [8a](#slice-8a8b8c-and-9-ownership-plan) ->
[8b](#slice-8a8b8c-and-9-ownership-plan) ->
[8c](#slice-8a8b8c-and-9-ownership-plan) -> milestone 9 -> 10 -> 11 -> 12.
The 8a/8b/8c split does not renumber milestones 9-12.
Place the independent helper-contract supplement after the runtime slices and
before the later benchmark harness checkpoint; it is not a runtime dependency.

### Historical slice 8a/8b/8c and 9 construction checkpoints

The following checkpoints predate the current publication section below;
their pending/held states are historical, not current published status.

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
not an exploit claim. Named policies share policy configuration, not bucket
budgets: buckets are listener-local. Unchanged continuous bindings retain
counters for the same budget; a changed budget or unbinding/rebinding creates
a fresh local bucket. Unused owners preserve identity, not retired token
history. There is no aggregate policy bucket. The earlier
shared/unused-budget preservation promise was incorrect. Preserve policy-owner
lifetime and retain correct
ownership across failed resume and subsequent cleanup/retry. Source
implementation has started on the old slice-8c base. The recommended design
uses a module-owned policy-owner map to eliminate runtime borrowing from
controller snapshots. Historical 195/72 pin changes are intentionally not
copied if that ownership proof holds; this remains a design obligation, not
already-proven final-candidate behavior.

## Earlier evidence and remaining-work record (superseded above)

### Earlier publication and remaining local work

The nine-branch atomic push completed with exact published heads verified.
Slice 7 now targets slice 6a. All twelve PRs are draft; the explicitly approved
intermediate-only validation override was used only for this authorized
publication. Main remains c62d84ad735404e23990806c53c882b0057b866c.

| Slice / PR | Current published head | Base |
| --- | --- | --- |
| 1 / #7683 | 8bffa5860639278644459224a256ac1991982477 | main / c62d84ad7 |
| 2 / #7684 | e33ad9a4d850bdc37c330dcd78ed011c7ea7e283 | #7683 / 8bffa5860 |
| 3 / #7685 | 04debdb95ce690e1b6569a1d1af34fda065d96e1 | #7684 / e33ad9a4d |
| 4 / #7686 | 88b9882f4ff818517e745b6314fef223bf38151a | #7685 / 04debdb95 |
| 5 / #7687 | 52d752dad4cbda74f8e392faa193f0f7606fca14 | #7686 / 88b9882f4 |
| 6 / #7689 | 0de7ab8e3cf8a5bbc06740d17ef07ad42a58ea7d | #7687 / 52d752dad |
| 6a / #7691 | 4e10c15dd2342be7f7bd1d1c73448b7ef041240d | #7689 / 0de7ab8e3 |
| 7 / #7690 | 1cc47d91800926cb7c9462d8f167490317701ca4 | #7691 / 4e10c15dd |
| 8a / #7692 | 434c0d2a0ea7f88f527a2c6c6bd41599ca3347ce | #7690 / 1cc47d918 |
| 8b / #7693 | d1ac0c3ac3ffd8b653a7350077f5afba3519d341 | #7692 / 434c0d2a0 |
| 8c / #7694 | 33356ca522e9fe48a89f7df240ac4315362ecf22 | #7693 / d1ac0c3ac |
| 9 / #7695 | 0b22b3cef9784d4704d886c50b32b1d29774fc7d | #7694 / 33356ca52 |

Strict merge order: 1/#7683 -> 2/#7684 -> 3/#7685 -> 4/#7686 -> 5/#7687 ->
6/#7689 -> 6a/#7691 -> 7/#7690 -> 8a/#7692 -> 8b/#7693 -> 8c/#7694 ->
[9/#7695](https://github.com/rsyslog/rsyslog/pull/7695) -> planned 10 -> 11 -> 12
-> helper supplement -> benchmark. All twelve source PR descriptions are
synchronized with the full order and merge conditions. Parent first;
retarget after parent merge and reverify the delta. Do not merge umbrella
#7654 alongside this stack. Hosted new runs and review-thread decisions remain
pending; no thread-resolution or hosted-clean claim is made.

Publication-time passed receipts are historical evidence, not current merge
clearance: slice 8b now has a blocked receipt on the unchanged exact digest.
Introducing-slice fixes, dependent cascades and fresh reviews are required.
Slice 2 retains its
non-blocking diagnostic hardening note. Slice 8a retains one non-blocking
hardening tradeoff: per-session regex compilation cost is unmeasured, not a
confirmed vulnerability. Other pending local descendants need fresh receipts
after their final cascades.

Current merge holds and validation limits:

- Slice 2 has an ordinary graph-walker initialization correction in progress,
  following hosted ARM64 ASan startup feedback. Its two-file local change has
  a failing original unit control and passing corrected control; actual Clang
  21 ASan+UBSan unit, omruleset and queue controls passed 3/3. Independent
  ordinary review is pending; no original-platform reproduction claim.
- Slice 8a has a separate ordinary regex preparation bound-consistency fix
  underway. Its earlier cost tradeoff does not cover this new correction;
  no overflow claim is made.
- Slice 8b's current unchanged digest is blocked after independent validation
  and lead review. There is static proof, not a runtime PoC. Fix authorization
  is pending; details remain outside this tracked operational ledger.
- Slice 12 is dirty and incomplete. Two native checks passed, but the full
  compile is unfinished and no HUP validation is claimed. Production wiring
  is paused pending the explicitly requested direct user approval.
- All twelve hosted PRs remain draft with unresolved AI review work and no
  human review clearance. A hosted documentation Codecov failure is not a
  Sphinx build failure.

| Slice | Local checkpoint and bounded evidence |
| --- | --- |
| 3 | 04debdb95 fixes hosted Clang cast-alignment feedback. Actual GCC and Clang 21 production/native checks passed; two fresh review shards reported zero candidates and the receipt is current for this checkpoint. |
| 4 | Frozen 88b9882f4ff818517e745b6314fef223bf38151a rejects modified rulesets whose optimizer-eliminated unnamed actions escape transfer auditing, while retaining unchanged-unnamed-action positives in both frontends. Focused controls 12/12, ASan 4/4 and mock distribution passed; two fresh security review shards reported zero candidates and the receipt is current. The earlier RS red control wrongly activated; the YAML red control terminated by signal, with cause unproven and no core. This is a correctness correction, not a demonstrated security issue. |
| 5 | Frozen 52d752dad4cbda74f8e392faa193f0f7606fca14 has two fresh review shards reporting zero candidates and a current receipt; 2,150 reviewable changed lines. |
| 6 | Frozen 0de7ab8e3cf8a5bbc06740d17ef07ad42a58ea7d includes the MMJSONPARSE test gate, verified through generated TESTS absence/presence controls. The full minimum Clang build and six focused controls passed; two fresh review shards reported zero candidates and the receipt is current for 3,691 reviewable changed lines. |
| 6a | New frozen 4e10c15dd2342be7f7bd1d1c73448b7ef041240d implements operational retry backoff/status separately from slice 6 ownership safety. Eight current focused controls passed; patch-identical pre-cascade evidence includes epoll 19/19, poll 19/19, mock distribution and HTML. Fresh discovery and lead reviews reported zero candidates and the receipt is current (9 files, 443 reviewable changed lines). |
| 7 | Published 1cc47d91800926cb7c9462d8f167490317701ca4 corrects unchanged-limiter budget reset on unrelated profile changes. Earlier epoll 22/22, poll 22/22, native red-guard evidence and a clear ordinary independent review are retained separately; fresh discovery and lead review completed with a passed receipt. |
| 8a | Published 434c0d2a0ea7f88f527a2c6c6bd41599ca3347ce has a fresh passed receipt with the non-blocking operational hardening tradeoff stated above. |
| 8b | Published d1ac0c3ac3ffd8b653a7350077f5afba3519d341 now has a blocked receipt, superseding the earlier passed review on the same digest. Earlier focused/backend/sanitizer evidence is retained, not remediation proof. Fix authorization and a subsequent fix/review/cascade remain pending. |
| 8c | Published 33356ca522e9fe48a89f7df240ac4315362ecf22 is integrated onto slice 8b with interface 39. Actual epoll 17/17, poll 17/17, mock distribution 1,127 passes/6 skips and HTML passed. Fresh discovery/lead reviews covered all 12 files, with zero candidates and a passed receipt. |
| 9 | Published draft #7695 at 0b22b3cef9784d4704d886c50b32b1d29774fc7d, parent 33356ca522e9fe48a89f7df240ac4315362ecf22. Actual epoll 17/17, poll 17/17, mock distribution 1,130 passes/6 skips and HTML passed, using the same pinned image and build/check -j4/-j2. Fresh Luna/high discovery and lead reviews covered all 14 files with zero candidates and a passed receipt. Earlier f18e413 evidence remains historical. |
| 10 | Current local 2d54d8699ae083a2104a6b3ba4c6b7d68ff9917e includes the testbench registration guard and corrected oversized-message helper/YAML oracles. Pre-cascade native GCC, direct Clang parser/owner, HTML and mock evidence remains separate. |
| 11 | Exact local f0b853059e48a938a274bef9efc5d2238ee4e990 passed combined epoll 42/42 (57.45s) and actual poll 42/42 (57.30s), with backend config.h, summary and actual ELF SHA evidence preserved. Corrected oversized-message helper/YAML controls passed 3/3; original failures remain. GCC ASan+UBSan O0 controls passed 3/3 with detect_leaks=0, not leak validation. Clang owner/catalog controls passed 2/2; its parser stops at an inherited function-ABI issue before tests. An extracted exact-main-prototype control reproduces that issue, not main-runtime behavior. GCC O1 was blocked by the inline heuristic; O0 sanitizer evidence is distinct. Mock distribution passed 1,186/6 skips (1,192 total); HTML passed in 154.63s. |
| 12 | Production all-worker readiness and early-stop lifetime work remains incomplete, with two native checks passed but full compile unfinished and no HUP validation. Direct user approval for source wiring is pending; no permission is inferred from continuation of other tasks. |

New local functional slice 6a separates operational retirement retry from
the ownership safety already implemented in slice 6: bounded attempts with
indefinite retry of owned pending state and visible pending/error status.
It is published as draft #7691; 8a, 8b and 8c are drafts #7692, #7693 and #7694.

Planned merge order is 6 -> 6a -> 7 -> 8a -> 8b -> 8c -> 9 -> 10 -> 11 -> 12
-> helper supplement -> benchmark. The final two follow runtime functionality.
The initial benchmark b500/78b5 checkpoints are superseded by frozen
d4fc5320c4ac9c0a587ce3f94077adc71393ca83 (8 files, 1,633 changed lines).
The package builder classified only two shell files/226 lines as reviewable;
independent review must cover all eight files. Task-local perf software
counters and record work, without global installation or host-setting changes,
but actual CSV reports all five hardware PMU events unsupported despite exit 0.
Final performance acceptance is blocked pending a user-provided capable host
or an explicit reduced-scope decision; this is not N/A or acceptance.
Slice 10 uses a portable
full-daemon parser-unit seam; this is not live-validation clearance.
Published receipt status must be read with the current holds above: slice 8b
is blocked, and slices 2 onward require introducing fixes/cascades and fresh
exact-candidate review before merge clearance. Earlier passed receipts do not
clear new findings or changed descendants.
Hosted checks, thread decisions
and final combined broad validation remain separate gates; the stack is not
overall merge-ready.

### Earlier receipt and validation audit trail

Read from each source worktree's ignored `.codex/security-review/receipt.json`;
Coverage rows retain complete reviewed/expected inventories; slice 8b is now
blocked, superseding its earlier passed status. Other publication-time passed
receipts do not override the current merge holds. Digests bind
the current published deltas, not the older historical tables below.

| Slice | Route | Reviewed/expected | Diff digest |
| --- | --- | --- | --- |
| 1 | lead_required | 34/34 | 54b151c42b5dab4f138769eeae3cd815dec138f58a8275cc33b65ea94416a969 |
| 2 | expanded | 28/28 | 51c1d765d8c13a6f8bda0383fc75c3f0ddab034e725d2cad5793bebbe3e47bd3 |
| 3 | expanded | 25/25 | 523878255c596a5ace06a167913e38427a9a45c48fe9889ed382878959b4ba0b |
| 4 | expanded | 42/42 | 3cb07a8e8d9ab71dda346be4e5f4a22550958ac2b71dd28e1690269418e06d5b |
| 5 | expanded | 17/17 | d476d13d02f31c17bd0c1123a5facca8398c012a11583b2973b3862b88ba56de |
| 6 | expanded | 34/34 | e48ac0134c84f78a3d864d13a429dcff50e7e5f06f070c419aa182b10756d730 |
| 6a | lead_required | 9/9 | d82d48ab051a5a2afa66290d86de83a4cd1655e783815564d8b55324a78eaed8 |
| 7 | lead_required | 22/22 | 978ac133bef1515f1034d1f4ef3b9645fab8e7e8d29074ba847e74e7d5c1d410 |
| 8a | lead_required | 13/13 | 11521f6abbe254ae799151225055de372b62ee34185a6fef83cefac7ce5bfdfe |
| 8b | lead_required | 14/14 | 2eed464864eba805e14715f7f66a1dbfea6a207ee7585c2c3fd21c157ba665e8 |
| 8c | lead_required | 12/12 | 7dd8f74372fb39fc65253b761d78ba1375b9837a0b6b3d4f0897d97f40eada26 |
| 9 | lead_required | 14/14 | 8c130dcf78656878b933e11ada17fba1e2af93d39ad8e9667dd298147e6e99e5 |

Local pending packages: slice 10 is lead_required, 32 expected files, digest
`566d8fcee49c70576848d9d4324ee45ff539a1f345f2f84f4e23b54f001c1125`;
slice 11 is lead_required, 24 expected files, digest
`51c3f53c92b6e9579411b69ff21564d388e58e9b3c43a238cf0bc3d86c5c641a`.
Neither has final security clearance; a further dependency cascade is pending.

Representative exact focused commands below run from the matching sibling
worktree. Common command prefix and configure flags are shared literally by
the three recorded lanes; TESTS selects the exact lists below. Source summaries:
S6a `stack06a-validation/final-handoff-summary.md`, S8b
`final-s6a-cascade/summary.md`, S8c `final-s8b-integration/summary.md`, each
under the worktree's ignored `.codex/security-review/`.

```bash
export RSYSLOG_DEV_CONTAINER=sha256:32ade478a405e4f27f077b5268ec5ecc59dd572843ad67ca2b6723594960ae09
export CC=gcc CFLAGS=-g CI_CONFIGURE_CACHE=1
export DOCKER_RUN_EXTRA_OPTS='--cpus=4 -e TESTS'
export RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE='--enable-testbench --enable-imdiag --enable-imtcp --enable-impstats --enable-omstdout --disable-valgrind'
# S6a final eight controls (31.36s):
TESTS='runtime_unit_shadow_reload_retirement runtime_unit_module_reload runtime_unit_tcpsrv_fence global-reloadonhup-on-rainerscript.sh global-reloadonhup-on-yaml.sh global-reloadonhup-unnamed-rainerscript.sh global-reloadonhup-unnamed-yaml.sh global-reloadonhup-imtcp-notifications-yaml.sh'
export TESTS
devtools/devcontainer.sh --no-pull --rm bash -lc 'if test -f Makefile; then make distclean; fi; devtools/run-configure.sh && make -j4 check TESTS="" && make -C tests -j2 check TESTS="$TESTS"'
# Combined S8b final nine controls (34.35s), same command/configuration:
TESTS='runtime_unit_tcpsrv_fence runtime_unit_reload_report runtime_unit_reload_candidate_graph global-reloadonhup-on-rainerscript.sh global-reloadonhup-on-yaml.sh global-reloadonhup-unnamed-rainerscript.sh global-reloadonhup-unnamed-yaml.sh global-reloadonhup-imtcp-allowedsender-rainerscript.sh global-reloadonhup-imtcp-allowedsender-yaml.sh'
devtools/devcontainer.sh --no-pull --rm bash -lc 'if test -f Makefile; then make distclean; fi; devtools/run-configure.sh && make -j4 check TESTS="" && make -C tests -j2 check TESTS="$TESTS"'
# S8c final seventeen controls, run separately with explicit yes and no:
TESTS='runtime_unit_tcpsrv_fence runtime_unit_reload_report runtime_unit_reload_candidate_graph global-reloadonhup-imtcp-maxlisteners-rainerscript.sh global-reloadonhup-imtcp-maxlisteners-yaml.sh global-reloadonhup-imtcp-maxlisteners-validate-rainerscript.sh global-reloadonhup-imtcp-maxlisteners-validate-yaml.sh global-reloadonhup-imtcp-maxsessions-rainerscript.sh global-reloadonhup-imtcp-maxsessions-yaml.sh global-reloadonhup-imtcp-regex-rainerscript.sh global-reloadonhup-imtcp-regex-yaml.sh global-reloadonhup-imtcp-allowedsender-rainerscript.sh global-reloadonhup-imtcp-allowedsender-yaml.sh global-reloadonhup-on-rainerscript.sh global-reloadonhup-on-yaml.sh global-reloadonhup-unnamed-rainerscript.sh global-reloadonhup-unnamed-yaml.sh'
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA=--enable-imtcp-epoll=yes
devtools/devcontainer.sh --no-pull --rm bash -lc 'if test -f Makefile; then make distclean; fi; devtools/run-configure.sh && make -j4 check TESTS="" && make -C tests -j2 check TESTS="$TESTS"'
export RSYSLOG_CONFIGURE_OPTIONS_EXTRA=--enable-imtcp-epoll=no
devtools/devcontainer.sh --no-pull --rm bash -lc 'if test -f Makefile; then make distclean; fi; devtools/run-configure.sh && make -j4 check TESTS="" && make -C tests -j2 check TESTS="$TESTS"'
```

The actual commands in persistent logs embed each TESTS list directly; the
shared-variable presentation above preserves the same flags and test selection.
S8c yes/no took 40.92s/39.22s and configured the requested backend explicitly.
S6a/S8b final focused composition is current; their earlier epoll/poll/sanitizer
runs remain pre-cascade evidence, not repeated current-head backend claims.
Mock distcheck is packaging-only: shell tests run MOCK-OK, not live coverage.
All lanes remain targeted, not final-tip broad/analyzer validation. Hosted AI
review/new checks are awaited and no review threads are claimed resolved.
The approved intermediate publication override covered the nine pushed refs.

| Later slice | Implementation/source locations |
| --- | --- |
| 9 | plugins/imtcp/imtcp-policy-owner.h and plugins/imtcp/imtcp.c own policy handles and listener-local binding state. |
| 10/11 | runtime/glbl.c, runtime/glbl.h, runtime/rsconf.c, runtime/parser.c, runtime/msg.c, tools/shadow_reload.c and tools/shadow_reload_policy.h implement bounded scalar lowering/publication and parser/message behavior. Real public-API native driver: tools/parser_policy_unit with tests/unit/parser_policy_test.c, invoked by tests/parser-policy-unit.sh; not a copied implementation-only surrogate. |

### Historical published quality batch (previous slices 1-7)

This section records local work after the published correction batch below.
It does not replace historical published heads, validation results, receipts,
or incident provenance. Slices 1-7 are published atomically with exact leases
and the explicitly authorized intermediate-only container-validation override.
Upstream and fork main remain c62d84ad735404e23990806c53c882b0057b866c.
At that publication, all seven had fresh passed receipts. Historical exact
candidate receipts are recorded below;
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

## Historical seven-PR publication checkpoint

This section preserves the earlier seven-PR order and heads. The current
twelve-PR table and strict order above supersede it.

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

Historical atomically published heads and bases (superseded):

| Slice / PR | Historical published head | Historical base |
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
