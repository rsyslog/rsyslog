# rsyslog container image family

This directory contains the build and packaging logic for the rsyslog
container image family:

- `rsyslog/rsyslog-minimal`
- `rsyslog/rsyslog`
- `rsyslog/rsyslog-collector`
- `rsyslog/rsyslog-dockerlogs`
- `rsyslog/rsyslog-etl`

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

## CI guidance

CI validation jobs should use non-release tags such as `ci-<sha>`.
Release publishing jobs should inject the stable release version
explicitly, for example `VERSION=2026-03`.
