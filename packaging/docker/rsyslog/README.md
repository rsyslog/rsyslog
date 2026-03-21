# rsyslog container image family

This directory contains the build and packaging logic for the rsyslog
container image family:

- `rsyslog/rsyslog-minimal`
- `rsyslog/rsyslog`
- `rsyslog/rsyslog-collector`
- `rsyslog/rsyslog-dockerlogs`
- `rsyslog/rsyslog-etl`

These are the only published image variants in this subtree.
The `host/` subdirectory contains auxiliary host-side forwarding
material and is not a built or published container image target.

Docker Hub descriptions for these repos can be maintained from this
subtree with `sync_dockerhub_metadata.py` and `dockerhub_metadata.json`.
The script reads credentials from `DOCKERHUB_USERNAME` /
`DOCKERHUB_PASSWORD` or the local `~/.docker/config.json` and defaults
to a dry run.

## Version and tag contract

Local builds default to a non-release tag on purpose:

```bash
make all
```

This produces images tagged with `dev-local`. The goal is to keep normal
local builds clearly separate from release artifacts.

Use an explicit version whenever you want to rehearse a release build
locally:

```bash
make all VERSION=2026-03
```

The build system treats `VERSION` as the source of truth for image tags.
Release automation must pass the intended stable version explicitly
instead of relying on the Makefile default.

Stable container tags are expected to follow the rsyslog release train:

- rsyslog release version `8.2602.0`
- container image tag `2026-02`

The stable-channel mapping rule is: `8.yymm.0` becomes `20yy-mm`.

## Publishing rules

Publish and `latest` tagging targets reject development-style versions:

- empty versions
- `dev-local`
- versions starting with `dev-`
- versions starting with `ci-`

This guard is intentional. It prevents accidental pushes of local or CI
validation builds.

Valid publishing examples:

```bash
make VERSION=2026-03 all_push
make VERSION=2026-03 push_latest
```

## PPA readiness comes first

Release-tagged container images must only be built and pushed after the
matching rsyslog packages are already available in the correct Adiscon
PPA.

This is a hard prerequisite because the image build installs rsyslog
packages from the PPA at build time. A release-like container tag is
incorrect if the requested rsyslog release series is not yet present
there.

By default, the manual release flow uses the stable channel:

- `RELEASE_CHANNEL=stable`
- PPA: `ppa:adiscon/v8-stable`
- tag mapping: `8.yymm.0` -> `20yy-mm`

Warning: this workflow assumes the selected PPA still publishes the
requested rsyslog release series. In practice, PPAs commonly expose only
the currently published package versions for a given Ubuntu series. Once
the PPA advances, older stable container sets may no longer be
rebuildable from it.

For the daily channel:

- `RELEASE_CHANNEL=daily-stable`
- PPA: `ppa:adiscon/daily-stable`
- image tag: `daily-stable`

Manual release targets therefore require:

- `RSYSLOG_VERSION` for the stable channel
- `RELEASE_CHANNEL=daily-stable` for the daily channel

The release readiness check resolves the newest matching `rsyslog`
package for the selected channel. The real image build remains the source
of truth for package completeness and will fail if required packages are
still missing.

The readiness check runs in a disposable Ubuntu container and does not
modify the host system's apt sources.

## Manual release flow

1. Determine the container tag from the rsyslog release tag.
   Example: `8.2602.0` and `v.26-02.0` both map to `2026-02`.
2. Verify PPA readiness:

```bash
make check_ppa_release_ready RSYSLOG_VERSION=8.2602.0
```

This looks up the newest `8.2602.0-*` package published in the Adiscon
PPA. If the PPA is not ready for that release series, the check fails
early. If subpackages are still missing, the actual image build fails.

3. Build the release-tagged image family:

```bash
make release_build RSYSLOG_VERSION=8.2602.0
```

4. Push the release-tagged images:

```bash
make release_push RSYSLOG_VERSION=8.2602.0
```

5. Update `latest` only when that is intended:

```bash
make release_publish RSYSLOG_VERSION=8.2602.0 PUSH_LATEST=yes
```

If `PUSH_LATEST` is not set to `yes`, `release_publish` pushes only the
versioned images and leaves `latest` unchanged.

For the daily channel:

```bash
make check_ppa_release_ready RELEASE_CHANNEL=daily-stable
make release_build RELEASE_CHANNEL=daily-stable
make release_push RELEASE_CHANNEL=daily-stable
```

## CI guidance

CI validation jobs should use non-release tags such as `ci-<sha>`.
Release publishing jobs should inject the stable release version
explicitly, for example `VERSION=2026-03`.
