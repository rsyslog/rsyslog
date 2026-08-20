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

## Publication invariants

- Build artifacts, package-pool objects, and dated snapshots are immutable.
  A collision with different content is a publication failure.
- Repository indexes and signed metadata are the only mutable objects.
  Upload immutable objects first and `InRelease` or signed RPM/APK metadata
  last.
- Metadata receives a short cache lifetime; immutable packages and snapshots
  receive a long lifetime.  CDN metadata refresh is expected and must not be
  blocked by an object policy that prevents required updates or deletes.
- Keep all published package versions and snapshots for at least five years
  from the first successful publication.  GitHub Actions artifacts are only a
  short-lived transport between jobs, not the archive of record.
- A package build alone is not success.  Every published target must verify
  public signed metadata from a clean matching environment, install the exact
  published package, and run the target's rsyslog smoke test.
- Scheduled failures create or update a distribution-specific GitHub issue.
  Manual recovery runs report through their Actions run and should be reviewed
  before enabling or changing a schedule.

## Package policy and feature contract

Target-specific policy files live in `.github/*daily-stable-policy.yml`.
They document every deviation from the native baseline, including additional
build dependencies, newly produced core files, and baseline-patch decisions.

RPM downstream patches require particular care.  A patch may only be removed
when it is explicitly reviewed as integrated or superseded in current `main`,
with its SHA-256 and rationale recorded in policy.  The generic validator is
`devtools/release/validate-rpm-baseline-patches.py`; do not restore broad patch
skip lists.

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
