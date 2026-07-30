# Amazon Linux daily stable package archive

The `amazon linux 2023 daily stable` workflow builds current rsyslog `main`
for Amazon Linux 2023 on `x86_64` and `aarch64`. It downloads Amazon's current official
rsyslog source RPM from the `amazonlinux-source` repository on every run and
uses that spec, configuration, systemd unit, dependency choices, and
subpackage split as the packaging authority. The source RPM's NEVRA and
SHA-256 are recorded with each build.

The explicit current-source deltas are recorded in
`amazonlinux2023-daily-stable-policy.yml`: already-upstreamed patches are
removed, dependencies for current YAML and impstats-push support are added,
and the current `mmleefparse` module is included in Amazon's core package.
Amazon's separate versioned 8.2204 HTML documentation tarball is deliberately
omitted instead of being relabeled as current documentation; current source
licenses, README, ChangeLog, and recovery-tool documentation remain in the
core package.

Package construction does not run for pull requests. Manual dispatch remains
available for bootstrap and recovery. The schedule is inactive until a manual
publication and clean Amazon Linux installation test pass.

## Archive layout and retention

The repository URL ends in:

```text
/rpm/daily-stable/amazonlinux/2023
```

The binary and source repositories are below that path:

```text
x86_64/Packages/*.rpm
x86_64/repodata/
aarch64/Packages/*.rpm
aarch64/repodata/
SRPMS/Packages/*.src.rpm
SRPMS/repodata/
```

Every publication also records its manifest, checksums, prepared spec, and
build log below:

```text
snapshots/YYYY-MM-DD/EVR/x86_64/
snapshots/YYYY-MM-DD/EVR/aarch64/
```

Each binary package is built and installed on a native runner of the matching
architecture.

RPMs and snapshots are immutable and retained for at least five years. Each
new repository generation merges prior signed metadata so old daily versions
remain installable. The EVR contains the UTC date, GitHub run and attempt,
and source commit. Only repository metadata, the public key, and the consumer
`.repo` file are mutable.

DigitalOcean's CDN currently enforces an observed one-hour minimum TTL even
when objects request a shorter cache lifetime. The workflow therefore verifies
that the CDN serves a valid signed repository, while exact newly published
version installation uses the public Spaces origin. Consumers continue to use
the CDN and can see a new daily version up to one hour later.

## Signing and configuration

The archive reuses the OpenPGP signing key and DigitalOcean Spaces credentials
already held by the protected `debian-daily-stable` GitHub Environment. Despite
its legacy name, that environment is the security boundary for the shared
package archive and is not limited to Debian.

Add this repository variable:

- `AMAZONLINUX2023_DAILY_STABLE_ENABLED`: `true` only after the first complete
  publication succeeds.

The scheduled workflow fails preflight if this switch is missing or is not
exactly `true` or `false`, so a configuration error cannot appear successful.

Add these variables to the `debian-daily-stable` environment:

- `AMAZONLINUX2023_DAILY_STABLE_REPO_URL`: public CDN URL ending in
  `/rpm/daily-stable/amazonlinux/2023`.
- `AMAZONLINUX2023_DAILY_STABLE_ORIGIN_REPO_URL`: public Spaces origin URL
  ending in `/rpm/daily-stable/amazonlinux/2023`.

No new private key, DigitalOcean account API token, or bucket permission is
required.

## End-to-end activation

1. Set the CDN and public-origin repository variables while leaving the
   schedule flag unset or false.
2. Run the workflow manually with publication disabled and inspect the RPMs.
3. Run it manually with publication enabled.
4. Confirm the CDN metadata signature and clean-install the exact new EVR from
   the public origin in native `amazonlinux:2023` containers for both
   architectures; verify all installed EVRs,
   `rsyslogd -v`, and `rsyslogd -N1`.
5. Set `AMAZONLINUX2023_DAILY_STABLE_ENABLED=true`.

The scheduled workflow opens or updates an issue if its build, publication,
or installation verification fails.
