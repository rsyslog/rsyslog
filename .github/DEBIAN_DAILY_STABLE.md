# Debian daily stable package archive

The `debian daily stable` workflow builds current rsyslog `main` for Debian 13
(`trixie`) on `amd64` and `arm64`. Package construction does not run for pull requests;
the repository's workflow lint and security checks still validate workflow
changes without consuming a full Debian package-build runner. Scheduled
publishing is enabled; manual dispatch remains available for recovery and
controlled verification.

The workflow owns package construction, APT metadata generation, signing,
and post-publication installation verification. The archive is served by
`packages.rsyslog.com` under a pull model (see
[DAILY_STABLE_PACKAGE_ARCHIVE.md](DAILY_STABLE_PACKAGE_ARCHIVE.md)): the
publish job uploads nothing to the server. It builds one signed publication
and uploads it as the Actions artifact `archive-publication-debian-13`; a
poller on the server verifies and applies it. The verification job then waits
for the server to report this run in `publication-status.json`, checks the
signed metadata from a clean Debian 13 container, installs the exact
published version, and smoke-tests the installed binary and default
configuration.

If a scheduled run fails or is cancelled, the workflow creates or updates the
`[debian-daily-stable] package archive failure` issue with the run URL and each
job result. Manual bootstrap runs report failures through their Actions run
without opening this daily-operations issue.

## Archive layout and retention

The repository URL is set in the workflow (`ARCHIVE_PUBLIC_URL`):

```text
https://packages.rsyslog.com/apt/daily-stable/debian/13
```

This leaves room for later channels, Debian and Ubuntu versions, and RPM-based
distributions. Debian packages and source
artifacts use immutable paths below `pool/`. Every run also records its
manifest, checksums, build information, and build log below:

```text
snapshots/YYYY-MM-DD/PACKAGE_VERSION/amd64/
snapshots/YYYY-MM-DD/PACKAGE_VERSION/arm64/
```

Signed APT indexes are published under both `binary-amd64/` and
`binary-arm64/`. Each package is built and installed on a native runner of the
matching architecture. `Release` and `InRelease` carry a `Valid-Until` 7 days
after generation (`ARCHIVE_RELEASE_VALID_SECONDS`), so the daily run must keep
the archive fresh; the verify job checks that the field is present.

During the retention window (see DAILY_STABLE_PACKAGE_ARCHIVE.md), the
APT indexes retain every published package version. Storage must not remove
package-pool, by-hash, or snapshot objects on its own; once pruning automation
exists, it must remove old versions from the indexes before deleting their pool
files and snapshots.

Only the small current `Packages.xz` and `Sources.xz` indexes are downloaded
before a publication, by
`devtools/release/debian-daily-stable.sh fetch-verified-indexes`. It verifies
the published `InRelease` with the archive key and checks each index against
the SHA-256 and size listed there. The run fails if a required index is
missing from the signed list or its download or integrity check fails. Only an `InRelease` answered with HTTP 404 (empty prefix) starts from
empty indexes. The indexes are merged with the new build, so daily Actions
traffic does not grow with the full archive size. The publication lists
immutable objects first, signed mutable metadata last, and `InRelease` last of
all.

## Setup contract

Configure these GitHub repository variables:

- `DEBIAN_DAILY_STABLE_ENABLED`: set to `true` only after a manual publication
  and installation test succeeds.
- `DEBIAN_DAILY_STABLE_GPG_FINGERPRINT`: full fingerprint of the archive
  signing key. It must stay a repository variable: the verify job has no
  environment.

All daily-package schedule switches are repository variables. A scheduled
workflow fails preflight, and therefore reports a failure issue, if its switch
is missing or is not exactly `true` or `false`.

Only the publish job uses the protected `debian-daily-stable` GitHub
Environment, which holds:

- `DEBIAN_DAILY_STABLE_GPG_PRIVATE_KEY`
- `DEBIAN_DAILY_STABLE_GPG_PASSPHRASE`
- `ARCHIVE_PUBLICATION_SIGNING_KEY`: ed25519 private key (OpenSSH format)
  that signs the publication manifest. Its public key is in the workflow
  (`ARCHIVE_PUBLICATION_PUBLIC_KEY`) and on the server.

The passphrase may be empty only if the archive key was intentionally created
without one. Keep stable-release publishing in a separate environment when it
is added later. This workflow no longer uses the `DEBIAN_DAILY_STABLE_SPACE_*`
settings or `DEBIAN_DAILY_STABLE_REPO_URL`; keep the Spaces settings, because
the targets still on Spaces share them.

## Provisioning and recovery

For a new archive environment, or after material archive-configuration
changes:

1. Run the workflow manually with publication disabled and review its package
   artifact.
2. Provision the server-side poller, DNS, signing keys, variables, and secrets.
3. Run it manually with publication enabled.
4. Confirm that the server applied the publication, and that signed metadata
   verification, clean Debian 13 package installation, and
   installed-configuration smoke test all pass.
5. Set `DEBIAN_DAILY_STABLE_ENABLED` to `true`.

To inspect a publication artifact locally, download it and run
`devtools/release/archive-publication.sh verify <dir> <allowed-signers>`
with an allowed-signers line
`rsyslog-archive-publication namespaces="rsyslog-archive-publication" <public key>`.

There is deliberately no long-term GitHub Actions artifact retention. The
short-lived Actions artifacts transfer one build between jobs and to the
server; `packages.rsyslog.com` is the durable archive.
