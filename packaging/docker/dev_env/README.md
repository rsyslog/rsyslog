These images are used for rsyslog CI and development testing. Each contains all
build dependencies so that the full feature set can be exercised. They are
intentionally large (1-3 GB) and do not attempt to optimize layers; easy
addition of build components takes precedence over size. As such they are not
intended for general production use. Developers working on rsyslog may use them
to reproduce the CI environment.

## Directory layout

- `ubuntu/`, `debian/`, `fedora/`, `centos/`, `suse/`, and `alpine/` mirror the
  long-standing container definitions used in CI.
- `openeuler/` contains development images based on the openEuler distribution.
  The initial `base/24.03-lts` image installs the full set of dependencies
  required to build rsyslog with the same feature coverage we exercise on other
  RPM-based platforms.
