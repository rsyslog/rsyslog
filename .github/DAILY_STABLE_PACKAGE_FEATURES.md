# Daily stable package feature contract

Daily stable packages start with the target distribution's native rsyslog
packaging. The shared contract in `daily-stable-package-features.yml` adds the
small Adiscon-maintained feature delta without replacing the native package
layout.

The prototype contract requires:

- libyaml-backed YAML configuration support in the base `rsyslog` package;
- a separately installable `omazuredce` module package; and
- published-package smoke tests that parse YAML and load `omazuredce`.

The overlay fails when a native baseline disables YAML, omits the distribution's
YAML development dependency, or no longer contains a unique structural anchor.
This makes native packaging drift a review event instead of silently generating
a different package.

Package names follow native conventions:

| Packaging family | Package |
| --- | --- |
| Debian, Ubuntu | `rsyslog-omazuredce` |
| Fedora, EL, Amazon Linux | `rsyslog-omazuredce` |
| openSUSE | `rsyslog-module-omazuredce` |
| Alpine | `rsyslog-omazuredce` |

The feature smoke test runs only after installing the exact published versions
of both the base and module packages. Scheduled workflow failures continue to
create or update the distribution-specific daily-stable failure issue.
