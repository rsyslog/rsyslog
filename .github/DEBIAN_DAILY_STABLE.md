# Debian daily stable package archive

The `debian daily stable` workflow builds rsyslog from `v8-stable` for Debian
13 (`trixie`) on `amd64`. Package construction does not run for pull requests;
the repository's workflow lint and security checks still validate workflow
changes without consuming a full Debian package-build runner. Manual dispatch
remains available for bootstrap and recovery. Scheduled publishing remains
disabled until the DigitalOcean archive is provisioned.

The workflow owns package construction, APT metadata generation, signing,
upload ordering, and post-publication installation verification. The
verification job uses the public CDN from a clean Debian 13 container, installs
the exact published version, and smoke-tests the installed binary and default
configuration. DigitalOcean Spaces is passive S3-compatible storage behind its
CDN; no package-building service runs there.

If a scheduled run fails or is cancelled, the workflow creates or updates the
`[debian-daily-stable] package archive failure` issue with the run URL and each
job result. Manual bootstrap runs report failures through their Actions run
without opening this daily-operations issue.

## Archive layout and retention

The initial repository URL ends in:

```text
/apt/daily-stable/debian/13
```

This leaves room for later channels, Debian and Ubuntu versions, RPM-based
distributions, and additional architectures. Debian packages and source
artifacts use immutable paths below `pool/`. Every run also records its
manifest, checksums, build information, and build log below:

```text
snapshots/YYYY-MM-DD/PACKAGE_VERSION/
```

The APT indexes retain every published package version. The Space must not have
a lifecycle rule that removes package-pool, by-hash, or snapshot objects before
five years. Retention starts with the first successful publication; the
workflow does not synthesize historical builds.

Only the small current `Packages.xz` and `Sources.xz` indexes are downloaded
before a publication. They are merged with the new build, so daily Actions
traffic does not grow with the full archive size. Immutable objects are
uploaded first, signed mutable metadata last, and `InRelease` last of all.

## DigitalOcean setup contract

Create one public-read Standard Spaces bucket with object versioning enabled.
Give the workflow a bucket-scoped read/write Spaces key; do not give it a
DigitalOcean account API token. Enable the Spaces CDN and route the final
package hostname to it.

Configure these GitHub repository variables:

- `DEBIAN_DAILY_STABLE_ENABLED`: set to `true` only after a manual publication
  and installation test succeeds.
- `DEBIAN_DAILY_STABLE_SPACE_BUCKET`: Space name.
- `DEBIAN_DAILY_STABLE_SPACE_ENDPOINT`: regional S3 endpoint, for example
  `https://fra1.digitaloceanspaces.com`.
- `DEBIAN_DAILY_STABLE_SPACE_REGION`: matching region, for example `fra1`.
- `DEBIAN_DAILY_STABLE_REPO_URL`: public URL including
  `/apt/daily-stable/debian/13`.
- `DEBIAN_DAILY_STABLE_GPG_FINGERPRINT`: full fingerprint of the archive
  signing key.

Create a protected `debian-daily-stable` GitHub Environment and add:

- `DEBIAN_DAILY_STABLE_SPACE_ACCESS_KEY`
- `DEBIAN_DAILY_STABLE_SPACE_SECRET_KEY`
- `DEBIAN_DAILY_STABLE_GPG_PRIVATE_KEY`
- `DEBIAN_DAILY_STABLE_GPG_PASSPHRASE`

The passphrase may be empty only if the archive key was intentionally created
without one. Keep stable-release publishing in a separate environment when it
is added later.

## Activation

Before enabling the schedule:

1. Run the workflow manually with publication disabled and review its package
   artifact.
2. Provision the Space, CDN, DNS, signing key, variables, and secrets.
3. Run it manually with publication enabled.
4. Confirm that signed metadata verification, clean Debian 13 package
   installation, and installed-configuration smoke test all pass.
5. Set `DEBIAN_DAILY_STABLE_ENABLED` to `true`.

There is deliberately no five-year GitHub Actions artifact retention. The
short-lived Actions artifact transfers one build between jobs; DigitalOcean
Spaces is the durable archive.
