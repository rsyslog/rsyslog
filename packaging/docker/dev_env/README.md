These images are used for rsyslog CI and development testing. Each contains all
build dependencies so that the full feature set can be exercised. They are
intentionally large (1-3 GB) and do not attempt to optimize layers; easy
addition of build components takes precedence over size. As such they are not
intended for general production use. Developers working on rsyslog may use them
to reproduce the CI environment.

## Directory layout

- `ubuntu/`, `debian/`, `fedora/`, `centos/`, `suse/`, and `alpine/` mirror the
  long-standing container definitions used in CI.
- `rocky/pkg_base/9` is the daily RHEL/CentOS/EPEL package-build host image
  (`rsyslog/rsyslog_dev_pkg_base_rocky:9`), successor to
  `rsyslog/rsyslog_dev_pkg_base_fedora:36`, with EPEL mock templates for
  EL8–EL10. See that directory's `README.md` for build/publish and daily-host
  cut-over notes (`run_daily_pkg_build.sh`).
- `openeuler/` contains development images based on the openEuler distribution.
  The initial `base/24.03-lts` image installs the full set of dependencies
  required to build rsyslog with the same feature coverage we exercise on other
  RPM-based platforms.
