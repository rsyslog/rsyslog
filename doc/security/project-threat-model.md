# rsyslog Project Threat Model

Model revision: 1
Schema version: 1
Status: reusable repository baseline

This document is the security context for local pull-request delta reviews. It
describes stable boundaries and invariants; it is not a list of vulnerabilities.
Scenarios below are review hypotheses until the evidence standard in
`doc/ai/security_triage_rubric.md` is met.

## 1. Overview

rsyslog is a modular logging daemon written primarily in C. Input modules turn
local or remote records into message objects, parsers and rulesets transform and
route those objects, queues decouple processing, and actions write or forward
the result. The documented pipeline is input to ruleset to action
(`doc/source/concepts/log_pipeline/index.rst:20-58`); plugins submit messages to
the main or ruleset queue through the core (`tools/rsyslogd.c:1238-1295`).

The daemon can start with elevated privileges to acquire resources and then
drop group and user privileges before input modules run
(`runtime/rsconf.c:971-985`, `runtime/rsconf.c:1067-1113`). Optional modules,
configuration, build flags, packaging, and deployment choices determine which
surfaces are present. Repository security scope includes the core, built-in and
contrib modules, and build or packaging logic that can affect released
artifacts (`SECURITY.md:15-31`).

| Component | Purpose | Representative evidence |
| --- | --- | --- |
| Inputs and sessions | Receive local files, sockets, journals, or network protocols | `plugins/imtcp/imtcp.c:249-315`, `plugins/imbeats/imbeats.c:242-260` |
| Message pipeline | Bound, parse, transform, queue, and route messages | `tools/rsyslogd.c:1238-1295`, `runtime/parser.c` |
| Transport security | Apply TLS authentication modes and peer restrictions | `runtime/nsd_ossl.c:650-725`, `runtime/nsd_gtls.c:1058-1117` |
| Configuration and modules | Select runtime behavior and dynamically loaded code | `runtime/modules.c:1163-1208`, `runtime/modules.c:1403-1420` |
| Storage and actions | Persist queues or write/forward messages | `runtime/queue.c:449-505`, `tools/omfile.c:364-399`, `tools/omfile.c:943-1058` |
| External helpers | Execute operator-selected programs and exchange message data | `plugins/omprog/omprog.c:143-222` |
| Build and release | Analyze, build, package, and publish artifacts | `.github/workflows/codeql.yml:25-44`, `.github/workflows/draft_release.yml:53-66` |

### Effective resources and authority

| Deployment or workflow | Resource or capability | Configuration and precedence | Readers, writers, or recipients | Enforcing control | Evidence or unknowns |
| --- | --- | --- | --- | --- | --- |
| Network collector | Listener socket and session state | Input instance, module defaults, then global stream-driver policy | Remote senders and the selected input module | Framing, session, message-size, rate, and optional peer controls | `plugins/imtcp/imtcp.c:341-351`, `plugins/imtcp/imtcp.c:445-529` |
| TLS collector or forwarder | Private keys, trust roots, peer identity | Operator-selected driver and authentication mode | rsyslog and configured peers | Driver certificate validation and permitted-peer matching | `runtime/nsd_ossl.c:650-725`, `runtime/nsd_gtls.c:1086-1117` |
| Privileged startup | Bound sockets, opened files, process identity | Command line and administrator configuration | Startup code, activated modules, then runtime workers | Ordered activation and `setgid`/`setuid` privilege drop | `tools/rsyslogd.c:1932-1953`, `runtime/rsconf.c:893-985` |
| Disk-backed processing | Queue, spool, state, and output paths | Administrator configuration below the effective work/output directory | Daemon workers and local administrators | Queue limits plus separately configured dynafile containment, final-symlink, and ownership controls | `runtime/queue.c:434-505`, `tools/omfile.c:364-399`, `tools/omfile.c:943-1058`; deployment permissions remain an assumption |
| Dynamic module loading | Shared-object search path and loaded code | Environment, command line, and configuration under the process authority | rsyslog process | Administrative control of environment, configuration, and module directories | `runtime/modules.c:1183-1208`, `runtime/modules.c:1403-1420` |
| Release automation | Source revision and release artifacts | Tagged or explicitly dispatched workflow | GitHub Actions and maintainers | Repository checks, pinned actions, and job permissions | `.github/workflows/draft_release.yml:20-65`; hosted repository settings are not visible here |

## 2. Threat Model, Trust Boundaries, and Assumptions

### Protected assets

- Process memory integrity and daemon availability while handling hostile data.
- Integrity, ordering, provenance, and intended routing of log messages.
- Confidentiality of message content, TLS keys, credentials, and configuration
  secrets handled by enabled modules.
- Peer identity when an operator explicitly configures authentication or sender
  restrictions.
- Queue, spool, state, and output-file integrity and durability.
- Administrative control of configuration, include files, module paths,
  helper programs, output destinations, and privilege settings.
- Integrity of source, CI workflows, packages, containers, and release
  artifacts.

### Actors and attacker capabilities

- **Remote unauthenticated sender:** can choose packet and stream bytes,
  framing, connection timing, message bodies, structured data, compression, and
  request volume on an exposed input. It does not start with configuration,
  host filesystem, or daemon privileges.
- **Authenticated or permitted peer:** has valid transport or sender
  credentials but may send malicious message content and resource patterns. Its
  identity does not make its payload trusted for parsing, paths, queries, or
  commands.
- **Local unprivileged producer:** may be able to write to a local log socket,
  FIFO, journal, or application file. It does not automatically control root-
  owned configuration, module directories, queues, or output directories.
- **Remote destination or helper:** receives data selected by the operator and
  can return malformed responses, withhold acknowledgements, or apply
  backpressure. The operator, not a log sender, normally selects it.
- **Pull-request contributor:** controls proposed repository content but does
  not inherently control protected branch settings, repository secrets, or
  release approval.
- **Administrator/operator:** controls configuration, module and helper paths,
  keys, destinations, and service permissions. Ordinary use of that authority
  is not a privilege gain; a finding needs a lower-privileged path across the
  boundary.

### Trust boundaries

### TB-NET-IN — Remote transport to input session

Remote bytes and connection behavior cross into listener, TLS, framing,
decompression, authentication, and protocol state. Controls must remain bounded
and must apply before data reaches sensitive parsing or allocation. imtcp caps
configured frame sizes and has secure-mode checks for TLS and zstd resource
policy (`plugins/imtcp/imtcp.c:341-351`, `plugins/imtcp/imtcp.c:445-529`);
imbeats exposes explicit frame, decompression, batch, in-flight, ratio, timeout,
and session limits (`plugins/imbeats/imbeats.c:1775-1810`).

### TB-LOCAL-IN — Local producer to daemon

Local sockets, files, journals, FIFOs, and program output cross from other
processes into the same message and parser pipeline. Locality changes exposure
and severity but does not make content memory-safe or semantically trusted.

### TB-CONFIG — Administrator policy to runtime authority

Configuration, YAML, includes, environment, module paths, credentials, helper
paths, output destinations, and privilege settings grant administrator-level
authority. Dynamic module loading reaches `dlopen`, and the module search path
can come from the environment before command-line override
(`runtime/modules.c:1183-1208`, `runtime/modules.c:1403-1420`). A report must
show how a less-privileged actor obtains that control under a realistic
deployment.

### TB-PRIV — Privileged startup to reduced-privilege runtime

Resources may be acquired before `setgid` and `setuid`; input modules run after
the configured drop (`runtime/rsconf.c:893-985`, `runtime/rsconf.c:1067-1113`).
The boundary must not retain unintended privileged operations or writable
control paths after the transition.

### TB-PIPELINE — Message data to parsing, rules, and queues

Untrusted fields become message properties, structured data, JSON, templates,
and queue entries. The core checks configured maximum message size before
enqueue and applies the configured split, truncate, or accept behavior
(`tools/rsyslogd.c:1247-1295`). Every parser or transformation retains its own
type, length, depth, ownership, and lifecycle obligations.

### TB-STORAGE — Daemon to queue, state, spool, and output files

Configured paths cross into filesystem operations and persistent state. Queue
types and disk/resource limits are configuration-dependent
(`runtime/queue.c:449-505`). omfile bounds normalized dynafile paths to a derived
static base, with an explicit administrator-controlled path-escape opt-in
(`tools/omfile.c:364-399`, `tools/omfile.c:1133-1138`). Separately, its effective
final-component symlink policy controls `O_NOFOLLOW`, and configured ownership
is applied with `fchown` when a file is created (`tools/omfile.c:939-1058`).
Strict dynafile policy also replaces unsafe message-derived path characters
(`runtime/template.c:100-158`). Host permissions and directory ownership remain
deployment controls rather than guarantees established by this repository.

### TB-OUTBOUND — Message pipeline to remote destinations

Actions transfer potentially sensitive or attacker-influenced content to
operator-selected network, database, broker, or cloud destinations. Templates
and protocol libraries must preserve data/syntax separation, and remote
responses must not create unbounded retry or resource behavior.

### TB-EXEC — Daemon to external helper process

Operator-selected programs execute with inherited runtime authority and receive
message data over pipes. omprog uses `execv` with configured arguments rather
than a shell (`plugins/omprog/omprog.c:143-222`); helper input parsing and
process lifecycle remain separate boundaries.

### TB-MODULE — Core to optional, contrib, and third-party code

Enabled modules execute inside the daemon or broker authority to external
libraries. Optional status reduces default exposure but does not remove an
enabled component from scope. Vulnerabilities in third-party libraries belong
upstream, while unsafe rsyslog integration remains in scope
(`SECURITY.md:17-31`).

### TB-CI — Pull-request content to build and release automation

Repository content is processed by lint, analysis, build, packaging, and
release workflows. Jobs must use minimal permissions and keep untrusted PR
execution separate from release credentials. CodeQL declares read-only source
permissions plus the security-event write needed for results
(`.github/workflows/codeql.yml:25-44`); release workflow authority is separate
and conditional (`.github/workflows/draft_release.yml:20-65`).

### Security invariants

### SI-INPUT-BOUNDS-01 — Bound hostile input work

Attacker-controlled length, nesting, compression, sessions, retries, file
descriptors, allocations, and CPU work must have effective limits before
resource exhaustion or memory-unsafe operations.

### SI-PARSER-SAFETY-01 — Parse with explicit extent and ownership

Parsers and transformations must preserve length, type, termination, depth,
allocation, and object-lifecycle invariants for every supported format.

### SI-PEER-IDENTITY-01 — Preserve configured peer identity

When authentication or sender restrictions are configured, the accepted peer
must be the peer validated by the effective driver and policy. Name,
fingerprint, certificate, address, and authenticated identity must not be
confused or replaced by message-controlled metadata.

### SI-TLS-EFFECTIVE-01 — Do not imply inactive transport protection

TLS-related configuration must either activate the expected transport and
authentication mode or produce the configured warning/error behavior. Plain
TCP/UDP and anonymous TLS do not provide authenticated sender identity.

### SI-CONFIG-AUTHORITY-01 — Keep runtime policy administrator-controlled

Unprivileged actors must not modify effective configuration, included files,
module search paths, loaded modules, credentials, helper paths, or privilege-
drop policy.

### SI-PRIV-DROP-01 — Complete the privilege transition

Configured UID/GID changes must complete before ordinary input processing, and
post-drop behavior must not regain or misuse startup authority.

### SI-MESSAGE-TRUST-01 — Keep message content untrusted

Authenticated transport proves a peer property, not the safety of message
fields. Message-controlled content must not silently become trusted provenance,
configuration, a filesystem path, query syntax, or executable arguments.

### SI-PATH-CONTROL-01 — Contain message-derived filesystem access

Every message-derived path component must use the applicable secure-path and
symlink policy; configured base directories, ownership, and permissions must
remain operator-controlled.

### SI-QUEUE-INTEGRITY-01 — Preserve queue durability and boundedness

Queue state, recovery, acknowledgement, discard, and disk limits must preserve
documented delivery semantics without unbounded resource use or unsafe local
tampering assumptions.

### SI-EXEC-DATA-01 — Separate helper selection from helper data

Only the operator selects executable paths and fixed arguments. Message data is
delivered as data, and helper failure, confirmation, timeout, and shutdown paths
must remain bounded.

### SI-SECRET-HANDLING-01 — Do not expose sensitive configuration

Keys, credentials, tokens, and private configuration must not be copied into
messages, diagnostics, public review artifacts, or release outputs.

### SI-CI-TRUST-01 — Keep untrusted source away from release authority

PR-controlled code must not gain write tokens, repository secrets, artifact
publication, or release authority without a separately authorized and
revision-bound workflow.

### Assumptions and exclusions

- Administrator configuration, module directories, keys, queue directories,
  and output directories are protected by deployment permissions. Violations
  require deployment evidence and are not inferred from a configurable path.
- Optional modules are exposed only when built, installed, loaded, configured,
  and reachable. Severity records those prerequisites.
- Plain syslog does not provide sender authenticity. Spoofing without a
  configured authentication boundary is expected behavior, not an auth bypass.
- Test and developer tools are not production surfaces by default, but build,
  test, packaging, or documentation logic remains in scope when it can affect a
  released artifact, consistent with `SECURITY.md:17-23`.
- Third-party library defects are reported upstream; rsyslog-owned validation,
  configuration, lifecycle, and API use remain reviewable.
- Distribution defaults, host MAC policy, service sandboxing, branch
  protection, and secret configuration are not fully observable from this
  repository. Missing deployment evidence lowers confidence; it is not proof
  that a code-level boundary is safe or exposed.

## 3. Attack Surface, Mitigations, and Attacker Stories

These are hypotheses for delta review, not confirmed vulnerabilities.

| Priority | Scenario and capability gain | Prerequisites | Impact | Existing controls | Review focus and evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Remote input turns malformed framing, compression, handshake, or message data into memory corruption or a cheap daemon crash | Enabled and reachable input | Availability or code execution | Per-input sizes, sessions, timeouts, secure-mode limits | Verify source-to-sink bounds and alternate encodings; `plugins/imtcp/imtcp.c:341-351`, `plugins/imbeats/imbeats.c:1775-1810` |
| 1 | A peer bypasses configured certificate, fingerprint, name, or sender restrictions and is accepted as trusted | Authenticated transport or ACL enabled | Forged trusted-source logs or broader compromise | Auth modes and permitted-peer matching | Trace the exact validated identity to accepted session; `runtime/nsd_ossl.c:650-725`, `runtime/nsd_gtls.c:1086-1117` |
| 1 | A lower-privileged actor changes an include, module path, helper, or effective policy consumed during privileged startup | Writable control path in a realistic deployment | Privilege escalation or arbitrary daemon code | Expected administrative ownership and privilege drop | Prove the lower-privileged write path and effective consumer; `runtime/modules.c:1183-1208`, `runtime/rsconf.c:971-985` |
| 2 | Message properties escape a dynamic output base or exploit an independently permissive final-symlink policy | Message-derived dynafile plus the relevant explicit policy or weak deployment permissions | File overwrite, file sprawl, or integrity loss | Secure-path replacement, base containment, and independently configured final-component `O_NOFOLLOW` | Check path rendering, base validation, opt-ins, and both opens separately; `runtime/template.c:100-158`, `tools/omfile.c:364-399`, `tools/omfile.c:939-1058`, `tools/omfile.c:1133-1138` |
| 2 | A sender exhausts sessions, decompression, DNS, queues, disk, retries, or downstream workers | Reachable input and insufficient effective limits | Denial of service or message loss | Input limits, rate controls, queue watermarks and disk caps | Establish resource asymmetry and effective configured limit; `runtime/queue.c:475-505` |
| 2 | Message content becomes query, JSON, template, or helper syntax without the required encoding boundary | Vulnerable template or downstream consumer | Forged output or downstream injection | Typed templates, escaping modes, `execv` helper launch | Separate operator-selected destination/program from message data; `plugins/omprog/omprog.c:143-222` |
| 2 | A PR changes a workflow so untrusted code reaches credentials or publication authority | Affected workflow and privileged trigger | Supply-chain compromise | Pinned actions, job permissions, separate release triggers | Trace event, checkout revision, permissions, secrets, and published digest; `.github/workflows/draft_release.yml:20-65` |
| 3 | Local producer input reaches a parser or state machine that assumed remote controls already ran | Writable local surface and affected parser | Local denial of service or integrity loss | Shared maximum-message and parser controls | Verify the local entry path uses equivalent bounds; `tools/rsyslogd.c:1247-1295` |

## 4. Severity Calibration

- **Critical:** clear, realistic default/common unauthenticated remote code
  execution; reliable attacker-controlled memory corruption with equivalent
  control; or a lower-privileged path that changes privileged configuration or
  loaded code in common packaging. A severe bug class without reachability and
  impact proof is not Critical.
- **High:** attacker-controlled memory corruption, major authenticated-identity
  bypass, privileged file replacement, or major integrity/confidentiality
  impact with a realistic enabled component and deployment. Optional status is
  a prerequisite, not an automatic suppression.
- **Medium:** meaningful integrity, confidentiality, routing, forgery, or denial
  of service that needs a non-default but realistic deployment, or a narrower
  local/optional surface without major privilege gain.
- **Low:** strong local or operator preconditions, low-impact defense in depth,
  diagnostic-only effects, or bounded hardening. Config-only behavior is
  normally administrator-equivalent unless a separate lower-privileged path is
  proven.
- **None/not actionable:** intended plain-transport behavior, already enforced
  controls, unreachable code, self-only effects, best-effort statistics, or
  missing attacker control/impact.

Final classification follows `doc/ai/security_triage_rubric.md`: a confirmed
issue needs a working reproducer or direct code proof of attacker-controlled
input, reachability, and impact. Potential issues and hardening guidance must
state the missing proof and must not use confirmed-vulnerability wording.
