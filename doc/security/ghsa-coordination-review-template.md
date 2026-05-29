# GHSA Coordination Review Template

This template is used by rsyslog maintainers to organize review and
coordination of private vulnerability reports. It is not an acceptance
checklist, severity formula, or guarantee of CVE assignment. Final advisory
wording, severity, affected versions, and publication timing depend on
maintainer validation and coordination with the reporter.

Use this template when reviewing a private GitHub Security Advisory report and
preparing a respectful coordination response to the reporter.

## Maintainer Workflow Notes

The GHSA coordinator is the maintainer team. At present, Rainer Gerhards
(`rgerhards`) is the primary coordinator and Andre Lorbach (`alorbach`) is the
fallback. The coordinator owns the reporter conversation, keeps the validation
state clear, and avoids parallel inconsistent replies.

Use the review prompt early. It is intended to help draft both the initial
reporter response and more substantial follow-up once maintainer validation
notes are available. The prompt output is a basis for maintainer review, not an
automatic decision. Maintainers still decide what to send, what to validate
next, and what wording needs reporter coordination.

For normal cases, the prompt should also be used as the first-pass validation
organizer. It should identify the affected component, default and non-default
configuration preconditions, strongest evidence, uncertainties, proposed
component-scoped wording, and next steps. Complex cases may need additional
manual analysis or separate technical review before a response is sent.

Keep the process proportional. A first acknowledgement normally only needs the
current status, missing information, and maintainer review before sending. A
substantive response or advisory draft should include claim labels,
evidence/confidence notes, publication blockers, and a downstream coordination
decision.

Do not make meaningful advisory wording changes unilaterally. Changes to the
title, impact, affected configurations, affected versions, or publication
timing should be coordinated with the reporter before publication, especially
when narrowing broad claims to a specific module or configuration path.

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
- The current coordinator is usually rgerhards, with alorbach as fallback.
  The coordinator owns the reporter conversation and avoids parallel
  inconsistent responses.
- The prompt may be used early for both an initial reporter response and later
  substantive follow-up. Treat this output as maintainer-draft material, not
  as an automatic acceptance, rejection, severity decision, or final advisory.
- Keep first acknowledgements lightweight: status, missing information, and a
  maintainer review before sending are usually enough. Use the fuller evidence/confidence,
  publication-blocker, and downstream-coordination sections for substantive
  responses or advisory drafts.
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
2. Organize the first-pass validation basis:
   - affected component, module, plugin, or configuration path
   - whether it is built in, optional, contrib, default-loaded, or explicitly
     configured
   - whether upstream defaults are affected
   - whether known distribution defaults differ from upstream defaults
   - whether required non-default settings are common in production or
     enterprise deployments
   - attacker control, vulnerable sink, missing guard, and demonstrated impact
3. Label each important claim as one of:
   - reporter claim
   - maintainer-confirmed fact
   - maintainer inference
   - open question
4. For substantive responses or advisory drafts, include an Evidence and
   Confidence table with:
   - claim
   - evidence
   - confidence: high, medium, or low
   - what would change the assessment
5. Summarize the strongest evidence in favor of the report.
6. Summarize the main uncertainties or wording concerns.
7. Propose component-scoped advisory wording for:
   - title
   - one-paragraph summary
   - affected configurations
   - impact
   - workarounds, if known
8. Identify any wording that should be coordinated with the reporter before
   publication.
9. Include a Downstream Coordination Decision for substantive responses or
   advisory drafts: likely needed, maybe needed, or likely not needed. Explain
   why based on default exposure, package availability, common enterprise
   configuration, severity, and embargo needs.
10. Include Publication Blockers for substantive responses or advisory drafts.
    List anything that must be resolved before publication, such as unclear
    affected versions, unvalidated impact, unconfirmed fix, missing workaround,
    uncoordinated wording changes, downstream coordination, or missing reporter
    credit preference.
11. Draft a reporter response that is professional, concise, and collaborative.
   If this is the first reply, acknowledge receipt and ask for missing
   validation details without promising acceptance, CVE assignment, severity,
   affected versions, or publication timing. If maintainer validation notes are
   provided, draft a more substantial follow-up that clearly separates
   confirmed facts from open questions.
12. Before the reporter response draft, include "Maintainer review before
    sending" with any sentences that are risky, premature, too broad, or need
    human confirmation.
13. Suggest next maintainer steps, including validation, duplicate handling,
   fix/review, CVE decision, release coordination, downstream coordination, and
   publication checks.

Output format:

## Assessment

## Claims, Evidence, and Confidence

## Proposed Advisory Wording

## Downstream Coordination

## Publication Blockers

## Maintainer Review Before Sending

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
