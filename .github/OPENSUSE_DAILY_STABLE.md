# openSUSE daily stable package archive

The `opensuse leap 16.0 daily stable` workflow builds current rsyslog `main`
for openSUSE Leap 16.0 on `x86_64`. Every run enables Leap's official disabled
source repository and downloads its current rsyslog source RPM. That native
spec, configuration, systemd unit, feature choices, file ownership, and
subpackage split are the packaging authority. The source RPM's NEVRA and
SHA-256 are recorded with each build.

The explicit current-source deltas are recorded in
`opensuse-leap160-daily-stable-policy.yml`. Current main additionally needs
`libyaml-devel`, `protobuf-c-devel`, and `snappy-devel`, and generates the core
`mmleefparse.so` module which the older native spec does not list. The workflow
adds only those declared deltas. It also explicitly installs four conditional
native BuildRequires (`libnet-devel`, `librdkafka-devel`, `net-snmp-devel`, and
`qpid-proton-devel`) which Leap's `zypper source-install` does not resolve, so
the native feature set is not silently reduced.

Leap packages documentation separately. Each run therefore builds the current
main-branch HTML and source documentation and replaces the native documentation
source tarball, preserving the `rsyslog-doc` package without shipping stale
documentation. The native Leap spec has no `%check` phase; the workflow keeps
that distro definition and compensates with a signed repository check followed
by a clean exact-version install and rsyslog configuration validation. If Leap
adds a downstream patch, the build stops for review instead of silently
dropping it.

Package construction does not run for pull requests. Manual dispatch remains
available for bootstrap and recovery. The schedule is inactive until a manual
publication and clean openSUSE installation test pass.

## Archive layout and retention

The repository URL ends in:

```text
/rpm/daily-stable/opensuse/leap/16.0
```

The binary and source repositories are below that path:

```text
x86_64/Packages/*.rpm
x86_64/repodata/
SRPMS/Packages/*.src.rpm
SRPMS/repodata/
```

Every publication also records its manifest, checksums, prepared spec, and
build log below:

```text
snapshots/YYYY-MM-DD/EVR/
```

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

- `OPENSUSE_LEAP160_DAILY_STABLE_ENABLED`: `true` only after the first complete
  publication succeeds.

The scheduled workflow fails preflight if this switch is missing or is not
exactly `true` or `false`, so a configuration error cannot appear successful.

Add these variables to the `debian-daily-stable` environment:

- `OPENSUSE_LEAP160_DAILY_STABLE_REPO_URL`: public CDN URL ending in
  `/rpm/daily-stable/opensuse/leap/16.0`.
- `OPENSUSE_LEAP160_DAILY_STABLE_ORIGIN_REPO_URL`: public Spaces origin URL
  ending in `/rpm/daily-stable/opensuse/leap/16.0`.

No new private key, DigitalOcean account API token, or bucket permission is
required.

## End-to-end activation

1. Set the CDN and public-origin repository variables while leaving the
   schedule flag unset or false.
2. Run the workflow manually with publication disabled and inspect the RPMs.
3. Run it manually with publication enabled.
4. Confirm the CDN metadata signature and clean-install the exact new EVR of
   `rsyslog`, `rsyslog-module-ossl`, and `rsyslog-doc` from the public origin in
   `opensuse/leap:16.0`; verify all installed EVRs, `rsyslogd -v`, and
   `rsyslogd -N1`.
5. Set `OPENSUSE_LEAP160_DAILY_STABLE_ENABLED=true`.

The scheduled workflow opens or updates an issue if its build, publication,
or installation verification fails.
