# Security Policy

## Reporting a Vulnerability

Please report security vulnerabilities through GitHub
[Private Vulnerability Reporting](https://github.com/rsyslog/rsyslog/security/advisories/new).
This is the preferred and canonical channel because it reaches the
maintainers without public exposure.

Do not use the public issue tracker for security vulnerabilities.

If GitHub Private Vulnerability Reporting is not suitable, contact the
maintainer directly at **rgerhards@adiscon.com**.

## Scope

The following areas are in scope for security reports:

- rsyslog core daemon code
- built-in input, output, parser, message-modification, and support modules
- `contrib/` modules, including network-facing modules such as `imhttp`
- test, build, and packaging logic when it can affect the security of
  released rsyslog artifacts

The following areas are out of scope:

- third-party libraries such as OpenSSL, librelp, or libcurl; please report
  those issues to their respective upstream projects
- vulnerabilities that require prior local access without crossing a meaningful
  security boundary
- versions that no longer receive upstream rsyslog releases

## Supported Versions

Security fixes are applied to the current development branch and included in a
subsequent release. Backports to older versions are normally handled by
downstream distribution maintainers.

The current stable release is listed on the
[rsyslog download page](https://www.rsyslog.com/downloads/).

## Response

rsyslog is maintained by a small team. Security reports receive priority
handling, and we aim to provide an initial response within 90 days where
possible.

## Requirements for a Valid Report

To allow efficient triage and remediation, reports should include:

- a working, end-to-end reproducer against the current stable release or the
  main development branch
- reproduction steps that exercise actual rsyslog code paths, not simulated or
  abstracted behavior
- a clear description of the security boundary that is violated
- the conditions required to trigger the vulnerability
- relevant configuration, platform, build options, logs, stack traces, or
  sanitizer output when available

Reports that do not meet these requirements may be deprioritized or returned
for clarification. Reports generated solely by automated scanners without
manual verification or an accompanying proof of concept may also be
deprioritized or returned for clarification.

## Coordinated Disclosure

We ask reporters not to disclose vulnerabilities publicly before an agreed
date. We work with reporters on a coordinated disclosure timeline, typically
within 90 days. Extensions may be agreed upon when the complexity of the fix or
the coordination effort requires it.

When a fix requires distribution packaging updates, we coordinate with relevant
Linux distribution security teams before disclosure where practical.

## CVE Assignment

CVEs are requested through the GitHub advisory process once a fix is confirmed
and reviewed. We keep the reporter informed throughout this process.

## Credit

Reporters are credited in the published security advisory unless they request
otherwise.
