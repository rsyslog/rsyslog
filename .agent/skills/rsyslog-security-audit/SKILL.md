---
name: rsyslog-security-audit
description: Run a deep security/vulnerability audit of an rsyslog component (input, parser, message modification, output, or runtime subsystem) with per-input-surface threat modeling, worst-case offset/length tracing of every write, ASan/UBSan reproduction with hostile inputs, and chain analysis. Findings stay private. Use for "security review of <module>", vulnerability hunting, or audits beyond a PR delta.
---

# rsyslog Security Audit

Use this skill for a full security review of one rsyslog component or
subsystem: an input module, a parser, a message-modification module, an output,
or a runtime area such as message properties, string handling, queues, or
transport drivers. For reviewing only the delta of a pull request, use
[`rsyslog-security-pr-review`](../rsyslog-security-pr-review/SKILL.md) instead;
this skill may still be applied to a risky PR when a maintainer asks for a deep
audit of the touched component.

The method is deliberately exhaustive. Surface-level pattern scanning misses
the bugs that matter in a C logging daemon: an offset that grows across several
copies in one function, a length check performed on a different value than the
one later used, or a "harmless" hardening item that feeds a later write.

## Ground rules

1. **Confidentiality comes first.** rsyslog/rsyslog is a public community
   repository. A real or suspected vulnerability must never appear in a public
   issue, pull request, PR or review comment, commit message, branch name,
   tracked file, CI log, or public chat. See
   [Reporting and confidentiality](#reporting-and-confidentiality).
2. **Model and effort.** Run the audit on the strongest available reasoning
   model at high effort. Do not delegate the per-write tracing (Phase 2) or the
   chain analysis (Phase 4) to a lighter or faster model to save cost; that is
   exactly where shallow passes miss real issues. Lighter models may help with
   mechanical tasks such as building or running prepared reproducers.
3. **Review independently.** Form your own conclusions from the source. Do not
   anchor on earlier reviews, tool output, issue text, or hints about where a
   bug "probably" is; read them only after your own pass, then reconcile.
4. **Proof over pattern.** Follow
   [`doc/ai/security_triage_rubric.md`](../../../doc/ai/security_triage_rubric.md).
   A risky-looking pattern is a hypothesis until source, reachability, sink,
   missing guard, and impact are established.
5. **Do not change behavior during the audit.** Audit the current source of the
   requested revision. Fixes are a separate, explicitly authorized step.

## Required reading

- [`SECURITY.md`](../../../SECURITY.md): scope, triage criteria, configuration
  preconditions, and coordinated disclosure.
- [`doc/ai/security_triage_rubric.md`](../../../doc/ai/security_triage_rubric.md):
  classification, proof standard, severity, and public wording.
- [`doc/security/project-threat-model.md`](../../../doc/security/project-threat-model.md)
  and the component's entry in
  [`doc/security/threat-model-components.json`](../../../doc/security/threat-model-components.json):
  actors, trust boundaries, invariants, and test hints.
- The component's source, its module documentation under `doc/source/`, and
  the shared runtime helpers it calls (for example `runtime/msg.c`,
  `runtime/stringbuf.c`, `runtime/datetime.c`, `runtime/parser.c`).

## Phase 1: Scope and threat model per input surface

Record the audited revision (`git rev-parse HEAD`), the component, and its
exposure: built in, default-loaded, explicitly configured, contrib, or
testbench-only.

Enumerate **every** input surface, not only the obvious socket read. For each
surface record: entry function and `file:line`, the actor that controls it
(per the threat-model actor list), the maximum size that can arrive, framing
and delimiters, privilege level at that point (before or after the privilege
drop), and the configuration preconditions needed to reach it.

Typical surfaces by component class:

- **Inputs (`im*`):** payload bytes from `recv`/`recvmsg`/`read`/library
  callbacks; ancillary data and kernel-supplied metadata (credentials, peer
  addresses, socket names); values derived from the payload such as hostname,
  tag, PID, or timestamps; reverse DNS results; TLS peer certificate fields;
  file names and file content (imfile); journal fields (imjournal); compressed
  streams and their decompressed size; connection and session counts.
- **Parsers (`pm*`, `runtime/parser.c`):** the raw message buffer and its
  length, and every offset the parser advances; partial or missing header
  fields; numeric fields that become lengths or indexes.
- **Message modification (`mm*`):** message properties, JSON trees, regex
  captures, lookup-table keys, and external data sources.
- **Outputs (`om*`):** template-rendered strings (attacker-controlled
  properties reaching SQL, JSON, HTTP, command lines, or file paths through
  dynafile templates); responses, error bodies, and acknowledgements from
  remote destinations or helper programs.
- **Runtime:** property getters and setters, string builders, datetime
  parsing, RainerScript functions on message data, queue disk files and spool
  state, transport drivers.

Configuration is administrator-controlled (TB-CONFIG). Still record which
settings widen exposure, for example a larger `maxMessageSize`, an option that
enables an extra parsing path, or relaxed TLS authentication, because the
severity and advisory wording depend on them.

## Phase 2: Per-write worst-case tracing

Inventory **every** write into fixed-size or pre-sized storage in the audited
code and in helpers it calls with attacker-influenced data:

- `memcpy`, `memmove`, `strcpy`, `strcat`, `strncpy`, `strncat`, `strlcpy`;
- `sprintf`, `snprintf`, `vsnprintf` (the return value is the would-be length,
  not the written length; check every place it is used as an offset or length);
- indexed stores such as `buf[i++] = c`, NUL termination `buf[len] = '\0'`,
  and loops that copy or unescape character by character;
- stack arrays, `alloca`, struct members with fixed sizes, and heap buffers
  sized once and filled later;
- reads that may run past the end (`buf[len]`, `strlen` on unterminated data,
  lookahead such as `p[1]`), which matter for crashes and information leaks.

For each write, determine:

1. The destination and its **real** capacity: `sizeof` versus the allocated
   size versus a constant used elsewhere. Confirm the constant matches the
   declaration that is actually in effect.
2. The offset expression and the length expression, each traced back through
   **the whole function**, earlier loop iterations, and callers to its origin.
   Note which terms are attacker-controlled and which depend on configuration.
3. The worst-case `offset + length` an attacker can reach, compared with the
   capacity. Do the arithmetic explicitly.
4. Every guard on the path: what value it checks, and whether that is the same
   value used at the write. A check on `len` does not protect a write at
   `offset + len` when `offset` has already advanced.

Patterns to look for in rsyslog code:

- an offset accumulated across several copies (header pieces, then a prefix,
  then the message), where each copy is bounded on its own but the sum is not;
- `len - offset` or `size - used` underflow when the offset can exceed the
  length, turning into a huge `size_t`;
- signed `int` lengths compared against unsigned sizes or converted to
  `size_t`, including negative values from parsed numeric fields;
- integer overflow in allocation sizes (`n * size`, `len + extra`), more
  likely on 32-bit builds;
- off-by-one in terminator space, or a loop that advances an index for
  skipped or escaped characters without rechecking the bound;
- a "validated earlier" assumption made in a different function or module,
  or one that holds only for the default `maxMessageSize`;
- truncation by `snprintf` followed by use of its return value.

Also cover the non-memory classes that apply to the component: format-string
use of message data, path traversal or symlink issues in derived paths,
command or query injection through templates, unbounded allocation or CPU
(decompression ratios, regex backtracking, session counts), NULL dereference on
missing fields, lifecycle bugs such as use-after-free between session close and
worker threads, data races, TLS verification or permitted-peer bypass, and log
forgery (control characters, injected record separators, spoofed trusted
properties such as hostname, PID, or credentials-derived fields).

Keep a working table outside the repository tree, one row per write:

| file:line | destination (capacity) | offset origin | length origin | worst case | guard | verdict |
| --- | --- | --- | --- | --- | --- | --- |

A write is cleared only when its worst case is proven to fit. "Probably fine"
stays open and moves to Phase 3.

## Phase 3: Dynamic verification with ASan and UBSan

Build an instrumented tree in a separate worktree or scratch directory, never
in the coordination checkout. Mirror the `ubuntu_26_san` lane in
`.github/workflows/run_checks.yml`; with the CI dev container that is roughly:

```sh
export CC=clang
export CFLAGS="-g -O1 -fno-omit-frame-pointer \
	-fsanitize=address,undefined -fno-sanitize-recover=undefined \
	-fno-sanitize=function"
export LDFLAGS="-fsanitize=address,undefined -fno-sanitize=function"
./autogen.sh --enable-debug --enable-testbench --enable-imdiag \
	--enable-omstdout --disable-libfaketime --disable-valgrind \
	--without-valgrind-testbench <module enable flags>
make -j"${RSYSLOG_LOCAL_BUILD_JOBS:-10}" check TESTS=""
export ASAN_OPTIONS='abort_on_error=1:halt_on_error=1'
export LSAN_OPTIONS='detect_leaks=0'
export UBSAN_OPTIONS='print_stacktrace=1'
```

The `function` sanitizer is excluded because the module ABI uses generic
callback pointers. Check that the build actually contains the audited module.

For **each** open or suspicious write from Phase 2, craft inputs that drive it
to its worst case, and run them against a real rsyslogd instance configured
like a plausible deployment (start from a testbench config via
`tests/diag.sh`). At minimum cover:

- **Oversize fields:** each attacker-controlled field larger than the buffer it
  lands in, at exactly the capacity, and at capacity plus or minus one; whole
  messages at and above `maxMessageSize`, with both default and enlarged
  settings.
- **Minimal inputs:** empty payload, a single byte, only a delimiter, only a
  header, a header without a message.
- **Truncated fields:** a header cut in the middle of a field, a missing
  terminator or closing bracket, a length prefix claiming more bytes than are
  present, and a stream that ends mid-frame.
- **Hostile values:** embedded NUL bytes, non-UTF-8 and control characters,
  extreme or negative numeric fields, deeply nested structures.

Use the tools that fit the surface: `tests/tcpflood` for TCP/TLS/RELP framing,
small Python scripts for UDP, Unix sockets, or raw bytes, `imdiag` injection
for pipeline-only paths, and the existing libFuzzer targets described in
[`tests/fuzz/README.md`](../../../tests/fuzz/README.md)
(`--enable-fuzzing`). A throwaway libFuzzer harness around a single parsing
function is often the fastest way to exercise one write in isolation.

Rules for this phase:

- Mark every finding **tested** (an ASan/UBSan report, crash, or observed
  misbehavior reproduced against actual rsyslog code paths) or **untested**
  (static reasoning only). Never present an untested finding as reproduced.
- When ASan reports an overflow, confirm the impact on a production-like build
  as well (`-O2 -D_FORTIFY_SOURCE=2`, no sanitizers): ASan changes memory
  layout, and fortified builds may turn the overflow into an abort.
- Keep reproducers, crash inputs, and logs outside the repository tree or in an
  ignored scratch directory. Never commit or push them during the audit.

## Phase 4: Chain analysis

Re-examine every hardening item and every cleared-but-close write, especially
the write-related ones, and ask whether it combines with something else into a
real vulnerability:

- Does an off-by-one, unchecked return value, or over-advanced offset feed a
  **later** write or length elsewhere in the same function or in a caller?
- Does a "configuration only" precondition become realistic in common
  deployments (enlarged message sizes on central collectors, an option that
  distributions enable)?
- Does a read overflow leak memory into an output, a forwarded message, or a
  log file that an attacker can read back?
- Does a denial-of-service item become memory corruption on a different
  architecture (32-bit `size_t`), compiler, or library version?
- Can two individually bounded inputs (for example a header field and the
  message body, or two ancillary values) together exceed a bound?

Reclassify any item that chains into a vulnerability, and record the chain
explicitly.

## Phase 5: Classification and output

Classify every finding using the rubric:

- **Potential vulnerability:** the rubric's *confirmed issue* or *potential
  issue*: plausible or proven attacker-controlled path to memory corruption,
  privilege or file-integrity impact, confidentiality loss, authentication
  bypass, or meaningful denial of service. State which one.
- **Hardening:** robustness improvement where exploitability is config-only,
  deployment-dependent, testbench-only, or not demonstrated after chain
  analysis.
- **Not actionable:** attacker control, reachability, or impact does not hold.

Report one line per finding, most severe first:

```text
[VULN|HARDENING] path/file.c:LINE  <sink and missing guard>; <attacker and preconditions>; <impact>; tested|untested
```

Follow the list with:

- the chains found in Phase 4, if any;
- a short "not actionable" list of investigated and cleared hypotheses;
- coverage: audited revision, input surfaces covered, which writes were only
  reviewed statically, build and configurations used, and what was out of
  scope. Absence of findings is a statement about this coverage, not proof that
  the component is safe.

## Reporting and confidentiality

- Deliver the full report only through a private channel to the requesting
  maintainer. Potential vulnerabilities go through GitHub Private Vulnerability
  Reporting or the maintainer contact in `SECURITY.md`.
- **Never** create a public issue, pull request, PR comment, discussion, commit,
  or branch on rsyslog/rsyslog that describes or hints at a potential
  vulnerability. That includes suggestive branch names, test names, commit
  messages, and "harmless" refactors that obviously fix it.
- Fixes for vulnerabilities are prepared on a private maintainer branch or a
  GitHub security advisory fork and are merged publicly only when coordinated
  disclosure allows, as described in `SECURITY.md`.
- Hardening items may go through normal public PRs only when chain analysis
  showed that they do not contribute to any vulnerability finding, and only with
  the rubric's public wording. If in doubt, keep the item private and ask the
  maintainer.
- Do not store findings in tracked files, public CI logs, or shared memory that
  other contributors can read; keep working notes outside the repository.

## Common pitfalls

- Stopping after the first bug in a function. Finish the trace; a second write
  with a different worst case is common.
- Checking the guard that exists instead of the arithmetic at the write.
- Assuming library or kernel data (DNS answers, ancillary data, certificate
  fields, remote responses) is well-formed or bounded.
- Reporting a finding as reproduced when only the static trace exists.
- Treating configuration control as attacker control, or the reverse: a
  non-default setting can still be a realistic exposure.
- Leaking a finding through a public artifact while asking for help or
  preparing a fix.

## Related skills

- [`rsyslog-security-pr-review`](../rsyslog-security-pr-review/SKILL.md):
  security review of a PR delta.
- [`rsyslog_build`](../rsyslog_build/SKILL.md) and
  [`rsyslog_test`](../rsyslog_test/SKILL.md): building and running focused
  tests.
- [`rsyslog_local_container_testing`](../rsyslog_local_container_testing/SKILL.md):
  CI-equivalent sanitizer lanes in the dev container.
