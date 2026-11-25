# SBOM for rsyslog source releases

`generate-sbom.py` writes a [CycloneDX 1.6](https://cyclonedx.org/) SBOM
(software bill of materials) for the `make dist` source tarball.  The draft
release workflow (`.github/workflows/draft_release.yml`) runs it for every
release and uploads `rsyslog-<version>.tar.gz.cdx.json` next to the tarball.
For tagged releases it also creates signed GitHub artifact attestations
(Sigstore) for the tarball's build provenance and for the SBOM.

These files are maintainer tooling. The tarball carries the generator and
the manifest only so that `tests/sbom-generator.sh` also runs under
`make distcheck`.

## What the SBOM contains

- **rsyslog itself**: version, license expression, purl
  `pkg:github/rsyslog/rsyslog@v<version>`, SHA-256 of the tarball.
- **Vendored code** (`[[vendored]]` in `sbom-components.toml`): code copied
  into the tree, with license, supplier and file locations.
- **External libraries** (`[[dependency]]`): every library that
  `configure.ac` or a non-test `Makefile.am` can link.  They are not in the
  tarball and their version is whatever the builder has installed, so the
  SBOM records only the name, purl where one exists, the minimum versions
  required by `configure.ac` (`rsyslog:sbom:version-constraint`) and whether
  the library is always needed (`scope: required`) or only for optional
  modules (`scope: optional`).

This is a source SBOM (`lifecycles: pre-build`).  Binary packages and
container images need their own build SBOM with the exact linked versions.

## Tests

`tests/sbom-generator.sh` (testbench, Python 3.11 or later) covers scanning,
`--check` drift detection, the CycloneDX output and `--check` on this tree.
Run it alone with `make check TESTS=sbom-generator.sh` or
`python3 tests/sbom-generator.py`.

## Keeping the manifest current

`sbom-components.toml` maps every pkg-config module and `-l<name>` that the
build system uses to one component.  When you add a library to
`configure.ac` or a `Makefile.am`, add its name to the `match` list of an
existing `[[dependency]]` or create a new one.  Check with:

```sh
python3 devtools/sbom/generate-sbom.py --check
```

The `SBOM manifest check` workflow runs the same check on pull requests
that touch the build system, the manifest or vendored documentation assets.
It also reports dependencies that are no longer used, vendored paths that no
longer exist, and vendored JavaScript versions that differ from
`doc/source/_static/vendor/README.md`.

## Running it locally

Python 3.11 or later, standard library only.

```sh
python3 devtools/sbom/generate-sbom.py \
    --version 8.2612.0 \
    --tarball rsyslog-8.2612.0.tar.gz \
    --output rsyslog-8.2612.0.tar.gz.cdx.json
```

Without `--version` the version comes from `AC_INIT` in `configure.ac`.
Set `SOURCE_DATE_EPOCH` for a reproducible timestamp; the serial number is
derived from the version and the tarball hash.

## Verifying a release

```sh
gh attestation verify rsyslog-<version>.tar.gz -R rsyslog/rsyslog
gh attestation verify rsyslog-<version>.tar.gz -R rsyslog/rsyslog \
    --predicate-type https://cyclonedx.org/bom
```
