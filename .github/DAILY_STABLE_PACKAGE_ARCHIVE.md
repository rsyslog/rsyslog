# Daily stable package archive program

## Purpose

The daily stable package archive delivers current rsyslog `main` through each
target distribution's native packaging system.  It is the integration testbed
for the package-release process: release packaging must reuse these policies,
package contracts, publication safeguards, and repository layouts rather than
creating a parallel packaging path.

Daily stable does **not** rebuild a distribution's historical rsyslog source.
It starts from the target distribution's current packaging baseline and
replaces its upstream source with current rsyslog `main`.  Native package
names, dependencies, service integration, configuration, file ownership, and
subpackage layout remain authoritative unless a narrowly reviewed policy
requires a current-upstream delta.

## Current targets

| Distribution | Version | Architectures | Archive prefix | Workflow |
| --- | --- | --- | --- | --- |
| Debian | 13 (trixie) | amd64, arm64 | `apt/daily-stable/debian/13` | `debian_daily_stable.yml` |
| Ubuntu | 26.04 (resolute) | amd64, arm64 | `apt/daily-stable/ubuntu/26.04` | `ubuntu_daily_stable.yml` |
| Enterprise Linux | 10 | x86_64, aarch64 | `rpm/daily-stable/el/10` | `el10_daily_stable.yml` |
| Fedora | 44 | x86_64, aarch64 | `rpm/daily-stable/fedora/44` | `fedora44_daily_stable.yml` |
| Amazon Linux | 2023 | x86_64, aarch64 | `rpm/daily-stable/amazonlinux/2023` | `amazonlinux2023_daily_stable.yml` |
| openSUSE Leap | 16.0 | x86_64, aarch64 | `rpm/daily-stable/opensuse/leap/16.0` | `opensuse_leap160_daily_stable.yml` |
| Alpine | 3.24 | x86_64, aarch64 | `apk/daily-stable/alpine/3.24` | `alpine324_daily_stable.yml` |

Extend support to further non-EOL versions and distributions by adding a
target-specific policy and a conservative workflow overlay.  Do not replace
the target's packaging layout with a one-size-fits-all package definition.

## Archive architecture

GitHub Actions builds, signs, publishes, and verifies packages.  DigitalOcean
Spaces is passive S3-compatible storage behind its CDN; it does not build
packages.  Workflows use the AWS CLI against the configured Spaces endpoint.
The shared `debian-daily-stable` GitHub Environment holds the archive signing
material and bucket-scoped Spaces credentials.

The archive uses the `rsyslog-packages` Spaces bucket in the `fra1` region.
Repository URL and endpoint values are GitHub repository variables so a target
workflow never hard-codes credentials or an account API token.  A bucket-scoped
read/write Spaces key is sufficient; do not require a DigitalOcean account API
token for regular publication.

The APT and RPM targets share one archive signing key; Alpine uses its own
RSA key.  Every APT and RPM publish job runs
`devtools/release/check-signing-key-expiry.sh` right after importing the key.
The job fails with a clear error when no usable signing key or subkey is left.
It warns when the key expires within 60 days.  Extend the key or replace it
before that, then update the private key secret and, for a new key, the
`DEBIAN_DAILY_STABLE_GPG_FINGERPRINT` variable.

## Move to packages.rsyslog.com

The archive is moving from Spaces to `packages.rsyslog.com`, a read-only
nginx tenant on the shared Docker edge of vserverweb5.adiscon.com.  The
hosting side is maintained in the private `Adiscon/infrastructure`
repository (`deploy/vserverweb5/`, runbook
`docs/runbooks/rsyslog-packages-onboarding.md`).  The two projects
coordinate only through `Adiscon/infrastructure` issue 17.  Decisions agreed
there on 2026-11-08:

- Storage goes through `devtools/release/archive-store.sh` with
  `ARCHIVE_BACKEND=ssh`.  Uploads never pass through Docker: they reach host
  sshd as user `rsyslog-archive`, whose key is bound to the forced command
  `archive-store-server.sh /home/otherweb/importdata/packages`.  That key can
  only `get`, `put-immutable` and `put-metadata`; it cannot delete.
- The installed server script is pinned at rsyslog commit
  `3969e63d080bcdd7cc7a7fd9ccb95cb64c3831a1`; its SHA-256 is
  `98230496b1265b453ac3b8f97cbb666d02aaf58424f8c282e910e147ec5ed1f1`.  A
  change to the script needs a new pin on the infrastructure side.
- Workflow settings per migrated target: `ARCHIVE_BACKEND=ssh`,
  `ARCHIVE_SSH_TARGET=rsyslog-archive@packages.rsyslog.com`,
  `ARCHIVE_PREFIX=<prefix>`, `ARCHIVE_SSH_KEY` (secret in the
  `debian-daily-stable` environment) and `ARCHIVE_SSH_KNOWN_HOSTS` (the line
  the infrastructure side hands over).  The prefixes stay the same as on
  Spaces.
- Debian 13 goes first with `ARCHIVE_PREFIX=apt/daily-stable/debian/13`.
  The other targets follow one at a time, only after Debian has run cleanly
  for a few nights.
- There is no dual publication.  Per target, a reviewed PR switches the
  publish job to the ssh backend and the public repository URL to
  `https://packages.rsyslog.com/<prefix>`.  Spaces stays untouched as a
  fallback until all targets are verified on the new host, then it is
  retired.
- The host only collects garbage.  A planned
  `devtools/release/archive-prune.sh <archive-root>`, run from a host timer
  and pinned like the server script, will delete pool files that no current
  index references and that are older than a grace period (default 3 days).
  It must parse APT `Packages`/`Sources` (including `by-hash`), RPM
  `repomd.xml` and the files it references, `APKINDEX.tar.gz`, and the dated
  snapshot indexes, so that no file a retained snapshot references is
  removed.  It offers a dry-run mode, logs every deletion and never touches
  metadata.  Until it exists, nothing is pruned.
- Verification per target stays with this project: signed metadata from a
  clean container, exact-version install and smoke test against
  `https://packages.rsyslog.com/<prefix>`, reported in issue 17.

## Publication invariants

- Build artifacts, package-pool objects, and dated snapshots are immutable.
  A collision with different content is a publication failure.
- Repository indexes and signed metadata are the only mutable objects.
  Upload immutable objects first and `InRelease` or signed RPM/APK metadata
  last.
- Metadata receives a short cache lifetime; immutable packages and snapshots
  receive a long lifetime.  CDN metadata refresh is expected and must not be
  blocked by an object policy that prevents required updates or deletes.
- Retention is the publisher's job and is expressed in the signed indexes:
  daily-stable package versions and snapshots are kept for 7 days during the
  proof of concept and for 90 days in production; scheduled-stable releases
  are kept indefinitely.  Pruning (not automated yet) must drop old versions
  from the indexes before their pool files and snapshots are removed.  GitHub Actions artifacts are only a
  short-lived transport between jobs, not the archive of record.
- A package build alone is not success.  Every published target must verify
  public signed metadata from a clean matching environment, install the exact
  published package, and run the target's rsyslog smoke test.
- Scheduled failures create or update a distribution-specific GitHub issue.
  The report quotes the likely cause from the failed job logs.  A repeat
  failure with the same cause updates the existing report's "still failing"
  line; a new cause adds a comment.  The reporter is
  `.github/scripts/report-daily-stable-failure.js`.  Manual recovery runs
  report through their Actions run and should be reviewed before enabling or
  changing a schedule.

## Package policy and feature contract

Target-specific policy files live in `.github/*daily-stable-policy.yml`.
They document every deviation from the native baseline, including additional
build dependencies, newly produced core files, and baseline-patch decisions.

RPM downstream patches require particular care.  A patch may only be removed
when it is explicitly reviewed as integrated or superseded in current `main`,
with its SHA-256 and rationale recorded in policy, or when the build proves it
integrated (below).  The generic validator is
`devtools/release/validate-rpm-baseline-patches.py`; do not restore broad patch
skip lists.

Distributions mostly carry backports of upstream commits, often several at
once after a security release.  To keep such routine backports from stopping
every nightly build, each build job writes an index of the packaged source
history (`RSYSLOG_UPSTREAM_COMMIT_INDEX`).  A patch that is not reviewed, or
whose reviewed content changed, is removed only when
`devtools/release/upstream_patch_detect.py` finds proof in its header: a
`git format-patch` origin, a cherry-pick or backport note, or a DEP-3 origin
naming a commit the packaged source contains, or else a subject identical to
an upstream commit subject.  Free-text commit mentions are not proof.  The
Debian and Ubuntu quilt loop applies the same rule to failing patches that are
not in `allowed_patch_skips`.  Every such removal emits a GitHub warning
annotation; fold it into the policy after review.  A patch without proof still
fails the build, as before.  A reviewed RPM entry that the baseline no longer
carries only warns, so the policy can be pruned later.

Current upstream may auto-enable an optional module (configure default
`optional`) that a native baseline predates; its files would then be
unpackaged.  RPM policies list such modules in `disabled_optional_modules`,
and Debian policies list their files in `not_installed_paths`.
`devtools/release/check-optional-module-policy.py` runs in the regular
testbench and fails the pull request that adds an auto-enabled module
without that policy.

The cross-distribution feature contract is
`.github/daily-stable-package-features.yml`; its human-facing explanation is
[DAILY_STABLE_PACKAGE_FEATURES.md](DAILY_STABLE_PACKAGE_FEATURES.md).  Current
requirements include YAML support in the base package and separately
installable OpenSSL, GnuTLS, `omotel`, and `omazuredce` packages.  The
`rsyslog-standard` and `rsyslog-full` profiles express the common rsyslog
product offering while retaining native package names and layouts.

## CI and release rules

Ordinary pull requests do not run expensive daily package builds.  They retain
workflow linting and security checks; daily and release workflows are the
package-build lanes.  Daily stable is the testbed for future scheduled release
automation: release additions must consume this common policy and verification
framework.

When modifying a target, validate end to end where publication credentials and
the target environment are available:

```text
build -> artifact checksum verification -> signed archive publication
      -> public metadata verification -> exact-version installation -> smoke test
```

Do not claim a target is operational from a local or build-only result.  Check
both configured architectures, investigate current workflow failures from their
logs, and treat a changed native package baseline as a review event.

## Detailed operational references

- [Debian 13](DEBIAN_DAILY_STABLE.md)
- [Fedora 44](FEDORA_DAILY_STABLE.md)
- [Amazon Linux 2023](AMAZONLINUX_DAILY_STABLE.md)
- [openSUSE Leap 16.0](OPENSUSE_DAILY_STABLE.md)
- [Alpine 3.24](ALPINE_DAILY_STABLE.md)
- Ubuntu and Enterprise Linux follow their named workflow and policy files;
  add comparable operational documents when introducing material new setup or
  recovery requirements.
