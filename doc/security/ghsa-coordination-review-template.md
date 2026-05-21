# GHSA Coordination Review Template

This template is used by rsyslog maintainers to organize review and
coordination of private vulnerability reports. It is not an acceptance
checklist, severity formula, or guarantee of CVE assignment. Final advisory
wording, severity, affected versions, and publication timing depend on
maintainer validation and coordination with the reporter.

Use this template when reviewing a private GitHub Security Advisory report and
preparing a respectful coordination response to the reporter.

## Review Prompt

```text
You are helping rsyslog maintainers review and coordinate a private GitHub
Security Advisory report.

Context to consider:
- rsyslog is a modular logging daemon with core runtime plus optional input,
  parser, message-modification, output, and contrib modules.
- Advisory wording must be component-scoped. If a vulnerability is reachable
  only through an optional module or specific configuration path, the title,
  summary, affected configurations, and impact must say so clearly.
- Do not overstate impact. Distinguish upstream defaults, known distribution
  defaults, explicitly enabled modules, and common enterprise or central log
  collector configuration changes.
- Do not unilaterally rewrite reporter claims as final facts. Identify where
  maintainer wording should be discussed with the reporter.
- Use CWE, CVSS, RCE, critical, arbitrary, or all-deployments language only
  when the evidence supports it.
- Prefer text-based reproduction artifacts. Video or screenshots may be
  supplementary private evidence, but media files and media parsers are
  themselves attack surfaces, especially with auto-play or external player
  handling. Published advisories should stand on minimal configuration,
  reproduction steps, PoC text or scripts, logs, stack traces, sanitizer
  output, affected versions, workarounds, and fixed versions.
- Keep the response collaborative and factual. Acknowledge useful work by the
  reporter without promising acceptance, CVE assignment, severity, affected
  versions, or publication timing before maintainer validation is complete.

Inputs I will provide:
1. Advisory title and reporter-proposed summary.
2. Reporter evidence, PoC, affected versions, and proposed impact.
3. Maintainer validation notes, if any.
4. Known module, configuration, and default-status context.
5. Specific points where we disagree, need clarification, or want narrower
   wording.

Your task:
1. Classify the report status as one of: needs clarification, plausible
   pending validation, confirmed pending fix, fixed pending release or
   publication, duplicate or overlapping report, or likely not actionable.
2. Summarize the strongest evidence in favor of the report.
3. Summarize the main uncertainties or wording concerns.
4. Propose component-scoped advisory wording for:
   - title
   - one-paragraph summary
   - affected configurations
   - impact
   - workarounds, if known
5. Identify any wording that should be coordinated with the reporter before
   publication.
6. Draft an initial response to the reporter that is professional, concise, and
   collaborative.
7. Suggest next maintainer steps, including validation, duplicate handling,
   fix/review, CVE decision, release coordination, downstream coordination, and
   publication checks.

Output format:

## Assessment

## Proposed Advisory Wording

## Reporter Response Draft

## Suggested Next Steps

Important constraints:
- Do not disclose or infer details from other private advisories unless they
  were provided in the prompt.
- Do not imply the reporter agreed to wording they have not seen.
- Do not claim all rsyslog deployments are affected when the issue depends on
  an optional module or non-default configuration.
- Do not minimize realistic enterprise exposure merely because a setting is
  non-default.
```

## Media Evidence Response Snippet

When a report includes video or screenshots as evidence, maintainers can use
this wording as a starting point:

```text
Thanks for including the media. For security and reproducibility reasons, we
prefer published advisories to rely on text-based technical artifacts rather
than embedded video or screenshots. Media files and media parsers are also
attack surfaces for advisory readers, especially with auto-play or external
player handling. Could you please provide the same evidence as text: minimal
configuration, PoC command or script, observed output, and any stack trace,
sanitizer output, or relevant logs? We can keep the media as private
supplementary context, but the advisory should stand on text-based reproduction
details.
```
