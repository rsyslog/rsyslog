# Security Triage Rubric for AI Agents

**Purpose:** Help AI assistants and automated reviews separate confirmed
security issues from potential issues, hardening opportunities, and invalid
findings.

**Audience:** Agents performing security review, hardening work, or PR
preparation in the rsyslog repository.

## Required Classification

Classify each finding before proposing or implementing a fix:

| Classification | Required evidence |
| --- | --- |
| Confirmed issue | Working reproducer, or direct code proof equivalent to a reproducer, demonstrating: attacker-controlled input, reachable code path, and impact. |
| Potential issue | Plausible source-to-sink path and plausible impact, but missing a reproducer or an important environmental proof. |
| Hardening | Robustness improvement where exploitability is config-only, deployment-dependent, testbench-only, best-effort stats related, or not demonstrated. |
| Not actionable | Attacker control, reachability, or impact does not hold after code review. |

Default to **hardening** when proof is incomplete.

## Proof Standard

Do not call a finding a vulnerability solely because a risky pattern appears in
the code. First prove the following:

1. **Source:** Identify whether the input is remote attacker-controlled, local
   user-controlled, configuration-controlled, testbench-controlled, or internal.
2. **Reachability:** Show the build option, module configuration, and runtime
   path needed to reach the code.
3. **Sink:** Name the exact write, read, allocation, parser, path operation,
   format operation, command boundary, or synchronization point that creates the
   risk.
4. **Missing guard:** Identify the absent or insufficient check, sanitizer,
   escaping step, size guard, atomic operation, or lifecycle pairing.
5. **Impact:** Demonstrate what changes for the attacker or operator: memory
   corruption, arbitrary file write, file read, message loss, forged output,
   denial of service, routing confusion, or only noisy statistics.

A working reproducer is preferred. Direct code proof can substitute for a
reproducer only when the source, sink, missing guard, and impact are explicit
and mechanically clear.

## CWE Use

Use CWE labels only for confirmed issues.

For potential issues, keep CWE mappings in internal notes unless the proof is
complete. If a CWE is obvious from direct code reasoning, still name the exact
source, sink, guard, and impact. Avoid public PR titles or descriptions that
claim a CWE when the finding is only hardening.

## Severity Guidance

Rate severity by demonstrated impact and realistic preconditions:

| Severity | Guidance |
| --- | --- |
| Critical | Default-reachable unauthenticated remote code execution, equivalent memory corruption with reliable control, or arbitrary privileged file write with realistic attacker control. |
| High | Attacker-controlled memory corruption, privileged file replacement, or major integrity impact with plausible deployment assumptions. |
| Medium | Meaningful integrity, confidentiality, denial-of-service, routing, or log-forgery impact that depends on a non-default but realistic deployment. |
| Low | Config-only, local-only with strong preconditions, testbench-only, noisy stats, diagnostic-only, or defense-in-depth hardening. |
| None | Not reachable, already guarded, intended behavior, or no demonstrated impact. |

Configuration control is usually administrator-equivalent in rsyslog. Treat
config-only behavior as hardening or low severity unless there is a separate,
realistic path for a lower-privileged actor to influence that configuration.

## Rsyslog-Specific Checks

Apply these checks before filing or implementing a finding:

- **Stats are best effort.** Atomic operations are acceptable on Linux. Do not
  add full mutex synchronization solely to make stats exact; noisy stats are
  preferable to hot-path contention.
- **Testbench modules are not product exposure by default.** Modules such as
  imdiag may intentionally expose diagnostic controls when explicitly enabled
  for tests. Treat misuse as operational risk unless default product exposure is
  demonstrated.
- **Config paths differ from runtime input paths.** A path chosen by the
  administrator is not automatically attacker input. For path findings, state
  who controls each path component.
- **Legacy behavior may be compatibility-controlled.** For breaking hardening
  defaults, prefer the global `compatibility.defaults.secure` policy:
  `strict`, `warn`, and `backward-compatible`.
- **Syntax compatibility is separate from runtime hardening.** Use
  `compatibility.configformat.*` only for legacy syntax controls, not for
  runtime security defaults.

## Test Expectations

Do not add placeholder tests.

Tests should exercise the changed path and fail without the fix when practical.
Register shell tests through `tests/Makefile.am`; do not create separate
recursive test harnesses below `tests/`. Prefer focused `diag.sh` tests over
large end-to-end suites unless the risk crosses module boundaries.

## Public Wording

Use wording that matches the proof:

- Confirmed issue: describe the demonstrated issue and impact precisely.
- Potential issue: say what remains unproven.
- Hardening: use terms such as "harden", "bound", "validate", "avoid",
  "preserve", or "make robust".

Avoid overstating exploitability. Do not use "critical", "RCE", "arbitrary",
or CWE labels in PR titles unless the proof standard is met.

## Inline Comments

Prefer agent-facing documentation for broad policy. Use inline comments only
when they preserve a local invariant that future maintainers could otherwise
break, for example:

- A non-obvious size calculation before appending variable-length metadata.
- A deliberate compatibility fallback controlled by
  `compatibility.defaults.secure`.
- A concurrency decision where stats are intentionally best-effort.
- A lifecycle pairing such as an initialization helper that needs destruction
  when atomic operations are disabled or unavailable.

Avoid inline comments that merely restate code or label a block as "security"
without explaining the invariant.
