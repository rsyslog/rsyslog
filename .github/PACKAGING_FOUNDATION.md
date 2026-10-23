<!-- SPDX-License-Identifier: Apache-2.0 -->

# Packaging program foundation

Recorded: 2026-10-23. This document turns the agreed packaging direction into
an implementation foundation. It does not add support claims or authorize
production migration. The existing [daily-stable archive program](DAILY_STABLE_PACKAGE_ARCHIVE.md)
remains authoritative for current publication and verification. This document
records the consolidation scope, first build-only candidate path and open work.

## Responsibility and sources of truth

The rsyslog main repository is the consolidation implementation home. New
selection, packaging workflows, native recipes, verification, publication
adapters, and release records belong here. Existing packaging repositories,
PPA/OBS services, archive endpoints, signing arrangements, and container image
flows remain operational until replacement beta passes and maintainers approve
cutover. No repository or service is retired by this foundation.

| Concern | Authoritative place | Responsibility |
| --- | --- | --- |
| Candidate and eventual supported targets | `.github/packaging-targets.json` | Distro/version, architectures, lifecycle evidence, enabled state, qualification, channels, builder/policy references and explicit exceptions. Candidate rows are not support claims. |
| Native baseline and distro deviations | `.github/*daily-stable-policy.yml` | Per-target native source, dependency and patch decisions. Keep distro-specific behavior reviewable. |
| Common feature offering | `.github/daily-stable-package-features.yml` | Shared module/profile contract consumed by `devtools/release/package-feature-overlay.py`; a profile is meaningful only when package contents and dependencies are verified. |
| Build and package behavior | Existing family builders under `devtools/release/` | Debian and Ubuntu share `debian-daily-stable.sh`; RPM, Fedora, Amazon Linux, openSUSE, and Alpine retain their native builder paths. Share proven helpers without hiding recipe differences. |
| Scheduled failure reporting | `.github/scripts/report-daily-stable-failure.js` | Reuse deduplicated actionable issues for public scheduled failures. Confidential preparation must not create public failure reports. |
| Current archive publication | Existing target workflows and `.github/DAILY_STABLE_PACKAGE_ARCHIVE.md` | Preserve signed metadata, immutable artifact ordering, public verification and install smoke tests while the storage backend evolves. |
| Supporting-library releases | Main-repository release-only component | Keep project libraries opt-in and independent from routine daily rsyslog packages. Start with librelp; confirm the remaining candidates before expanding. |

The catalog must eventually select actual jobs and supply their target inputs;
a list that only generates documentation or checks for drift is not operational
source of truth. The current seven archive workflows remain the starting build
inventory. Candidate invocations now select architectures and policies from the catalog,
within each native workflow's supported matrix. Scheduled and ordinary manual
runs retain their existing native inputs. Hosted parity remains to be demonstrated.

## First executable slice

The implementation adds `.github/workflows/packaging_candidates.yml` as a manual-dispatch, build-only
qualification selector. Its first scope is the seven targets with existing
daily-stable workflows. Resolve the selected public source ref once to an immutable full commit SHA, pass that
pinned source to the existing target builder paths, and explicitly disable
publication. A candidate run must never write to the daily or stable public
repository. Keep target-specific build, architecture, and verification behavior
in the existing workflows/builders rather than creating a second recipe engine.

Retain all 13 planned targets without existing archive routes in the catalog.
The selector lists their missing routes and rejects explicit requests for them. Do not silently omit them or
represent a planning row as qualified. Report the selected target, source SHA,
architectures actually exercised, builder result, and exact blocking phase for
each candidate. Candidate architecture selection is bounded by the native workflow matrix;
unknown architectures fail before a build starts. The catalog does not yet
control scheduled production coverage or qualify the resulting packages.

This first slice provides an executable path from catalog selection to existing
build requests. Hosted execution remains to be demonstrated after publication.
Dispatch acceptance alone is not a successful build or package verification.
The requesting automation SHA is recorded, but the child automation ref can move
between requests; inspect each child run SHA before accepting its artifacts.
Candidate requests share per-target concurrency with production and may queue it.
The dispatcher's parallelism limit bounds requests, not total child build jobs.
The next execution gate is a hosted candidate run using these adapters, followed
by verification of the exact produced packages. Do not add a generic workflow
framework before this reused path has been demonstrated.

## Candidate catalog integration

Each child workflow now accepts an optional `candidate_target`. Its preflight
confirms that the catalog row points back to that workflow, then supplies the
selected policy and architecture matrix. Combining a candidate target with
publication is rejected. An empty target preserves the existing scheduled and
ordinary manual behavior, including enabled/disabled decisions and publication.
The native matrix literal lives in preflight and is used directly for normal
runs; candidate runs filter those same rows using the catalog. Existing
publication and published-install jobs retain their matrices.

Use small native adapters: Debian and Ubuntu translate canonical `x86_64` and
`aarch64` to `amd64` and `arm64`; EL10 also preserves the corresponding CentOS
Stream plus EPEL mock configuration. Retain the current public runners. Policy
paths come from reviewed catalog routes, not arbitrary dispatch inputs. Fixed
suite, container and archive settings still belong to each native build path:
adding a catalog row alone cannot enable another distro release. The current
EL10 builder is CentOS Stream plus EPEL; the planned enterprise baseline choice
must not silently relabel that existing evidence as Rocky or RHEL qualification.

## Using the candidate entry point

From the repository checkout:

```sh
python3 devtools/release/package-candidate-plan.py --list
python3 devtools/release/package-candidate-plan.py --targets=debian-13,el-10
python3 devtools/release/package-candidate-plan.py --targets=existing
```

The manual **packaging candidates** workflow accepts the same `targets` values
and a public `source_ref`. The default requests only Debian 13 and EL10; `existing`
requests all seven routes. Explicitly requesting a target without a route fails
the whole plan before dispatch, so there is no silently partial request.

Once the workflow has been published and made dispatchable in the public
repository, use Actions' manual run interface or:

```sh
gh workflow run packaging_candidates.yml --repo rsyslog/rsyslog \
  --ref REVIEWED_AUTOMATION_REF \
  -f targets=debian-13,el-10 -f source_ref=PUBLIC_SOURCE_REF
```

Only maintainers with repository write access should invoke this entry point,
and only from reviewed automation refs. GitHub executes workflow definitions
from the selected ref, including the child workflows: the false publication
input is an operational safeguard in those reviewed definitions, not a security
boundary against modified workflow code. Do not dispatch unreviewed automation
branches. Public source revisions may be selected independently.

There is no publish input. The workflow resolves `source_ref` once via the public
repository, records the request artifact, and passes that SHA plus boolean
`publish_to_archive=false` to every child. There are no schedules, tag triggers,
signing credentials, private workers, or storage operations in this entry point.
Child build results remain in the existing target workflows; publication and
published-install verification are skipped. Do not call their build artifacts
qualified packages until the required candidate install tests have run.

## Release and verification paths

Routine public path:

```text
source commit or tag -> target-native build -> checksummed artifacts
  -> candidate verification -> immutable package upload
  -> signed mutable repository metadata -> public metadata verification
  -> exact published-package install and smoke test -> failure issue if scheduled
```

Keep one writer per target and channel. Upload package bytes and snapshots
before signed mutable indexes. Reuse the current archive workflows' install and
smoke verification, and preserve the last verified repository state when a new
build fails. Provider replacement is a publisher/backend change behind this
sequence, not a reason to duplicate builds or release policy. VPS/static hosting
with an independent backup is the accepted direction; provider, cost and restore
implementation remain open.

Daily, stable and emergency security releases are distinct invocations over the
same target-native recipes and feature policy. Daily publishes only to the daily
channel. Scheduled stable and ordinary public security tags build and publish
through public resources. Do not treat a daily build as a stable release or
reuse its mutable channel/version identity. Stable package revisions, ordering,
channel transitions and recovery need to be specified and verified before beta.
A failing target is reported independently and does not suppress verified
publication for other targets.

Private preparation is rare and needs explicit disclosure authorization. Reuse
the same recipes and tests, keep build access separate from signing credentials,
and promote the exact tested package bytes after disclosure; do not rebuild
changed source or add a private lookup to ordinary public releases. A
non-embargoed emergency security release follows the public path. Preserve the
existing release/advisory approval boundary.

## Supporting-library release graph

Use distro-provided libraries whenever adequate. The project-library route is
release-only and separately enabled; publishing a library must not replace an
adequate distro version for ordinary installations. Pilot librelp first with a
native package recipe, opt-in repository component, dependency-selection check,
and rsyslog RELP integration test. Then extend release archive coverage to liblognorm, libfastjson and libestr;
confirm the remaining supporting-library inventory, including liblogging.
Library release availability is separate from whether rsyslog needs to consume
that project version instead of an adequate distro library.

When a target needs a project library, build and verify its immutable package
first, record the exact package identity, then build rsyslog against that
identity. Otherwise resolve the distro library normally. No daily library
channel or blanket repository priority is part of the initial design.

## Small release record and implementation order

Keep a small manifest with the source SHA, target and architecture, native
packaging and policy revisions, build image and dependency inputs, package
checksums, workflow run links, and validation results. Reuse CI artifacts and
logs for detailed evidence; do not introduce a release database or evidence
service. Preserve package and snapshot retention obligations during any storage
migration.

Implementation order:

1. Land the manual, pinned-source, build-only candidate selector and explicit
   blocked reporting for planned targets; demonstrate it invokes existing
   target build paths with publication disabled in the reviewed workflows.
2. Demonstrate the implemented candidate architecture/policy adapters on hosted
   builders; preserve every current target and native architecture matrix.
   Fill lifecycle, access, migration and feature evidence as qualification proceeds.
3. Complete feature ownership and channel/version selection, then pilot the
   release-only librelp route and dependency graph.
4. Add public tag/scheduled stable and security orchestration, the manifest and
   publisher seam; rehearse failure recovery and private exact-byte promotion.
5. Measure hosting and archive growth, test backup restoration, and begin the
   agreed parallel beta only after target qualification and migration evidence.

The catalog records the program maintainer role; per-target named operational
ownership remains an open assignment before support admission. Do not invent
names or imply an unassigned target is operational. Service-manager integration
work is useful qualification evidence, but it is not a prerequisite to writing
this foundation or to implementing the build-only selector. No step in this
sequence shuts down legacy repositories, PPA/OBS builds, endpoints, or container
flows; cutover and retirement require a later explicit maintainer decision.
