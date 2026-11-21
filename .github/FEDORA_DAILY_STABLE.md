# Fedora daily stable package archive

The `fedora 44 daily stable` workflow builds current rsyslog `main` for
Fedora 44 on `x86_64` and `aarch64`. It downloads Fedora's current official rsyslog source
RPM from the Fedora 44 repositories on every run and
uses that spec, configuration, systemd unit, dependency choices, and
subpackage split as the packaging authority. The source RPM's NEVRA and
SHA-256 are recorded with each build.

The explicit current-source deltas are recorded in
`fedora44-daily-stable-policy.yml`: an explicit `autoconf-archive` build
dependency because the generated main-branch tarball does not bundle the AX
macros used by current `configure.ac`, and the `rsyslog-segqueue` binary and
manual page in the base-package file manifest because the Fedora baseline does
not yet list them. Fedora's feature selection, package split, configuration,
documentation, and service unit remain unchanged. If Fedora adds a downstream
patch, the build stops for review instead of silently dropping it.

Package construction does not run for pull requests. Scheduled publishing is
enabled; manual dispatch remains available for recovery and controlled
end-to-end verification.

## Archive layout and retention

The repository URL ends in:

```text
/rpm/daily-stable/fedora/44
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

RPMs and snapshots are immutable and retained for the retention period (see the retention rule in
DAILY_STABLE_PACKAGE_ARCHIVE.md). Each
new repository generation merges prior signed metadata so old daily versions
remain installable until
pruned. The EVR contains the UTC date, GitHub run and attempt,
and source commit. Only repository metadata, the public key, and the consumer
`.repo` file are mutable.

Fedora 44 is published at `packages.rsyslog.com` and uses the signed pull
model of the Debian archive (see DAILY_STABLE_PACKAGE_ARCHIVE.md). The publish
job no longer writes to the archive. It builds a signed publication, uploads it
as the artifact `archive-publication-fedora-44` (kept 7 days), and a poller on
the server verifies and applies it. The verify job waits up to 30 minutes for
`publication-status.json` to name its run, attempt, and manifest hash, then
checks the published repository and installs the exact new EVR from it.

Before generating the new repository, the publish job downloads the current `repomd.xml` of
`x86_64`, `aarch64`, and `SRPMS` over HTTPS with
`rpm-daily-stable.sh fetch-verified-metadata`. Each must verify against its
`repomd.xml.asc` with the archive signing key and no other key, and every file
it lists must match its SHA-256, and its size when `repomd.xml` gives one. HTTP 404 for a `repomd.xml`
starts that repository from scratch (expected for the first publication) only if
`<prefix>/publication-status.json`, which the server-side poller writes after
each applied publication, is 404 as well. If that file exists, the prefix was
published before and the missing `repomd.xml` fails the run, so an index with
only the newest build is never signed. For manual recovery (for example a newly
added arch) the operator may set `ARCHIVE_ALLOW_EMPTY_INDEX=true` for the helper,
which logs a warning and starts from scratch; workflows do not set it. Any other
failure stops the run. RPMs,
snapshots, and the SHA-256-named `repodata` files are immutable bundle entries;
the public key, the `.repo` file, `repomd.xml` and `repomd.xml.asc` are
metadata.  Each `repomd.xml` is immediately followed by its
`repomd.xml.asc`, and the bundle ends with a `repomd.xml.asc`.

## Signing and configuration

The archive reuses the OpenPGP signing key held by the protected
`debian-daily-stable` GitHub Environment and the
`ARCHIVE_PUBLICATION_SIGNING_KEY` secret that signs the publication. Despite
its legacy name, that environment is the security boundary for the shared
package archive and is not limited to Debian.
Add this repository variable:

- `FEDORA44_DAILY_STABLE_ENABLED`: `true` only after the first complete
  publication succeeds.

The scheduled workflow fails preflight if this switch is missing or is not
exactly `true` or `false`, so a configuration error cannot appear successful.

The verify job has no environment, so `DEBIAN_DAILY_STABLE_GPG_FINGERPRINT`
must stay a repository variable. The archive URL is fixed in the workflow.

No new private key or server credential is required.

## Provisioning and recovery

1. Leave the schedule flag unset or false.
2. Run the workflow manually with publication disabled and inspect the RPMs.
3. Run it manually with publication enabled.
4. Confirm the published metadata signature and clean-install the exact new
   EVR from `packages.rsyslog.com` in native `fedora:44` containers for both architectures;
   verify all installed EVRs,
   `rsyslogd -v`, and `rsyslogd -N1`.
5. Set `FEDORA44_DAILY_STABLE_ENABLED=true`.

The scheduled workflow opens or updates an issue if its build, publication,
or installation verification fails.
