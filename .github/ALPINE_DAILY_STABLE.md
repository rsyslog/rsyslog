# Alpine daily stable package archive

The `alpine 3.24 daily stable` workflow builds current rsyslog `main` for
Alpine Linux 3.24 on `x86_64`. It starts with Alpine's official
`3.24-stable` `main/rsyslog` APKBUILD and replaces only the upstream source
and daily version. This preserves Alpine's configure choices, dependencies,
subpackage split, OpenRC integration, and default configuration.
The only current policy addition is `autoconf-archive`, required by AX macros
used by current upstream but not by Alpine's released 8.2604 source package.

Package construction does not run for pull requests. Manual dispatch remains
available for bootstrap and recovery. The schedule is inactive until a manual
publication and clean Alpine installation test pass.

## Archive layout and retention

The repository URL ends in:

```text
/apk/daily-stable/alpine/3.24
```

The current `x86_64` repository is below that path:

```text
x86_64/APKINDEX.tar.gz
x86_64/*.apk
```

Every publication also records its manifest, checksums, prepared APKBUILD,
and build log below:

```text
snapshots/YYYY-MM-DD/PACKAGE_VERSION/
```

APK files and snapshots are immutable and retained for at least five years.
The workflow merges the previous signed index with each new package set so
older daily versions remain installable. Only the small mutable APK index and
public signing key use the CDN's 60-second metadata cache override.
The ordered APK version contains the UTC date and GitHub run/attempt serial;
the manifest records the exact rsyslog source commit.

## Signing

Alpine repositories use an RSA signing key, independently of the OpenPGP key
used by APT and RPM repositories. The protected `debian-daily-stable` GitHub
Environment is reused as the security boundary for the shared package archive.
Despite its legacy name, it is not limited to Debian packages.

Add this environment secret:

- `ALPINE_DAILY_STABLE_RSA_PRIVATE_KEY`: PEM-encoded RSA private key used by
  `abuild` to sign APK packages and their APKINDEX.

Add these repository variables:

- `ALPINE324_DAILY_STABLE_ENABLED`: `true` only after the first end-to-end
  publication succeeds.
- `ALPINE324_DAILY_STABLE_REPO_URL`: public CDN URL ending in
  `/apk/daily-stable/alpine/3.24`.
- `ALPINE_DAILY_STABLE_RSA_PUBLIC_KEY_SHA256`: SHA-256 of the PEM public key.

The existing shared Space bucket, endpoint, region, access key, and secret key
remain unchanged. A DigitalOcean account API token is not required.

## End-to-end activation

1. Generate and store the Alpine RSA key and its public-key SHA-256.
2. Run the workflow manually with publication disabled and inspect the APK
   artifacts.
3. Run it manually with publication enabled.
4. Confirm that a clean `alpine:3.24` container trusts the published key,
   installs the exact daily rsyslog version, and passes `rsyslogd -N1`.
5. Set `ALPINE324_DAILY_STABLE_ENABLED=true`.

The scheduled workflow opens or updates an issue if its build, publication,
or installation verification fails.
