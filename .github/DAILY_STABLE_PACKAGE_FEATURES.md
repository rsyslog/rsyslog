# Daily stable package feature contract

Daily stable packages start with the target distribution's native rsyslog
packaging. The shared contract in `daily-stable-package-features.yml` adds the
small Adiscon-maintained feature delta without replacing the native package
layout.

The prototype contract requires:

- libyaml-backed YAML configuration support in the base `rsyslog` package;
- separately installable OpenSSL, GnuTLS, `omotel`, and `omazuredce` module
  packages;
- an `rsyslog-standard` profile that pulls in the TLS drivers and `omotel`;
- an `rsyslog-full` profile that extends `standard` with `omazuredce`; and
- published-package smoke tests that parse YAML and load every profile module.

The overlay fails when a native baseline disables YAML, omits the distribution's
YAML development dependency, or no longer contains a unique structural anchor.
This makes native packaging drift a review event instead of silently generating
a different package.

The module package names follow native conventions:

| Packaging family | OpenSSL | GnuTLS | omotel | omazuredce |
| --- | --- | --- | --- | --- |
| Debian, Ubuntu | `rsyslog-openssl` | `rsyslog-gnutls` | `rsyslog-omotel` | `rsyslog-omazuredce` |
| Fedora, EL, Amazon Linux | `rsyslog-openssl` | `rsyslog-gnutls` | `rsyslog-omotel` | `rsyslog-omazuredce` |
| openSUSE | `rsyslog-module-ossl` | `rsyslog-module-gtls` | `rsyslog-module-omotel` | `rsyslog-module-omazuredce` |
| Alpine | `rsyslog-openssl` | `rsyslog-gnutls` | `rsyslog-omotel` | `rsyslog-omazuredce` |

Both profiles depend on the exact build version of the base and their selected
modules. Published verification installs `standard` before `full`, then checks
the installed version, module ownership, YAML parsing, and module loading.
Scheduled workflow failures continue to create or update the
distribution-specific daily-stable failure issue.
