<!-- SPDX-License-Identifier: Apache-2.0 -->
# Transactional reload implementation and PR-stack ledger

Recorded: 2026-10-15. Status: stack construction in progress, not published.
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

Open work: construct and validate each adjusted stack boundary, record PR URLs
and exact source/image evidence, review security per slice, and complete the
activation-mode release-performance gate. Only then begin milestone 4 (general
ruleset graph/call lifecycle). Current bounded reload is not full-config reload.
