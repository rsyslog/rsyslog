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
handling, and we aim to acknowledge reports within 7 days where possible.
Final fixes and coordinated disclosure timelines are handled separately, as
described below.

## Requirements for a Valid Report

To allow efficient triage and remediation, reports should include:

- the affected rsyslog component, module, plugin, or configuration feature
- whether the affected component is loaded by default or requires explicit
  configuration
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

## Initial Triage

rsyslog has a modular architecture. During triage, maintainers evaluate:

- the affected component or module
- whether the component is built in, optional, contrib, default-loaded, or
  explicitly configured
- whether exploitation works with upstream defaults
- whether known distribution defaults differ from upstream defaults
- whether required non-default settings are common in production or enterprise
  deployments
- who controls the input that reaches the vulnerable path
- the vulnerable sink, missing guard, and demonstrated impact
- whether the report includes a working reproducer or equivalent direct code
  proof
- whether the issue affects released rsyslog software, repository automation,
  test infrastructure, packaging, or documentation only

## Advisory Scope and Wording

When a vulnerability affects an optional module, plugin, contrib module,
parser, output, input, or specific configuration path, advisories should name
that affected component rather than describe the issue as applying to all
rsyslog deployments.

For example, an issue reachable only when a specific plugin is loaded and used
should be described as affecting that plugin. The affected package may still be
rsyslog, but the advisory will state the required module and configuration
conditions.

## Configuration Preconditions

Many rsyslog security reports depend on configuration. During triage, we
distinguish between:

- upstream default configuration
- distribution-provided default configuration
- explicitly enabled modules or plugins
- operator changes that are common in production deployments

A vulnerability that requires changing an upstream default, such as increasing
the maximum message size, is not treated the same as a vulnerability reachable
in a default installation. However, non-default settings may still represent
realistic exposure when they are common in enterprise or central log collection
deployments.

Advisories should state these preconditions clearly. For example, if an issue
requires a specific optional module and a larger-than-default message size, the
advisory should identify both conditions and avoid implying that all rsyslog
installations are affected.

## Reproduction Artifacts and Media

Reports should stand on text-based technical artifacts that maintainers,
downstream security teams, and users can review, archive, search, and
reproduce. Preferred artifacts include minimal configuration, reproduction
steps, proof-of-concept text or scripts, logs, stack traces, sanitizer output,
affected versions, workarounds, and fixed versions.

Videos and screenshots may be useful as supplementary private evidence, but
published advisories should not include embedded video. Media files and media
parsers are themselves attack surfaces, especially when content auto-plays or
is opened in different browser or player environments. Security advisory
readers should not need to open media files or rely on auto-playing content to
understand a vulnerability. Screenshots should also be avoided unless they add
essential context that cannot be represented as text, and any media must be
reviewed for sensitive environment details before publication.

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
