<!-- SPDX-License-Identifier: Apache-2.0 -->
# TEST ONLY / DO NOT MERGE: Debian rebuild experiment

This disposable PR replays Debian **8.2610.0-3**, not current rsyslog source.
It is a diagnostic experiment for the failures in reproduce.debian.net builds
[amd64 305459](https://reproduce.debian.net/amd64/api/v1/builds/305459/log),
[amd64 305339](https://reproduce.debian.net/amd64/api/v1/builds/305339/log), and
[arm64 536002](https://reproduce.debian.net/arm64/api/v1/builds/536002/log).

The dedicated workflow is gated to this experiment branch and runs native
amd64/arm64 jobs with the original architecture-specific buildinfo files.
`debrebuild --builder=sbuild+unshare` recreates the package sources, Debian
patches, recorded dependency versions, build path and environment. Original
parallelism is6 on amd64 and8 on arm64/armhf. CPU, host kernel, network and
outer bootstrap tools differ from the original builders, so this is not an
assertion of identical timing or a reproduction until a matching oracle fails.
Download SHA256 values are pinned; no GPG-signature validation is claimed.

Native execution requires devscripts (debrebuild/debootsnap), sbuild,
mmdebstrap, uidmap, debian-archive-keyring and ca-certificates. The runner must
allow ordinary unprivileged user namespaces. The script checks this and fails
before the build if unavailable; it never changes kernel or AppArmor policy.

```sh
devtools/test-debian-repro.sh amd64
```

For the2GiB ARMv7 ODROID, use the exact armhf source/dependency recipe with an
**explicit concurrency deviation**, after its current-main focused tests finish:

```sh
python3 devtools/debian-repro-experiment.py --arch armhf --parallelism 2 \
  --output /home/rger/work/debian-armhf-20261109
```

That mode preserves the original signed file and creates an unsigned derived
input changing only DEB_BUILD_OPTIONS parallelism. Both hashes and the override
are recorded in rebuild.json. It is not an exact environment replay. Do not run
original parallel8 on the memory-constrained ODROID.

Setup/configure/build failures remain distinct from actual test failures.
A phase begins only after a real test header or Automake result is observed.
A successful test phase followed by package-output differences does not create
flake evidence. Full rebuild logs and pinned inputs are retained as short-lived
experiment artifacts; binaries and dependency caches are not uploaded.

This harness has no production-source changes, no package publication and no
merge intent. It should be closed after the diagnostic result is recorded.
