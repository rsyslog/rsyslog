<!--
Thanks for your PR!

Commit Assistant (recommended for the commit message):
- Web (humans): https://www.rsyslog.com/tool_rsyslog-commit-assistant
- Base prompt (canonical): https://github.com/rsyslog/rsyslog/blob/main/ai/rsyslog_commit_assistant/base_prompt.txt

Important: put the substance into the **commit message** (not only here).
If needed, amend first (`git commit --amend`) and then open the PR.
-->

### Summary (non-technical, complete)
<!-- Why this change matters: modernization, maintainability, perf/security,
     Docker/CI readiness, or user value. This text should mirror the commit’s
     non-technical intro. -->

### References
<!-- Full GitHub URLs; use Fixes: only if conclusively fixed. -->
Refs: https://github.com/rsyslog/rsyslog/issues/<id>

### Notes (optional)
<!-- Mention tests/docs updates or planned follow-ups. If behavior changed,
     ensure the commit body has a one-line Impact and a one-line Before/After. -->

---

#### Quick check (optional)
- Commit message follows rules (ASCII; title ≤62, body ≤72; `<component>:`).
- Commit message includes non-technical “why”, Impact (if behavior/tests changed),
  and a one-line Before/After when behavior changed.
- Used the Commit Assistant or mirrored its structure.

---

#### Git workflow tips (optional, but helps reviews)
- Start by crafting the commit message locally (use the Assistant).
- If you already committed, improve it with:
  ```
  git commit --amend
  ```
- Squash related commits before PR where appropriate.
- Push your branch and then open the PR.
- If key info is only in the PR text, maintainers may ask you to move it
  into the commit message for a clean history.

