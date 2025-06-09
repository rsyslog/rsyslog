# Rsyslog Contribution Guide

Rsyslog is a real open source project, welcoming contributions of all kinds—from code to documentation to community engagement. Whether you're an experienced developer or a first-time contributor, there are many ways to get involved.

---

## Ways to Contribute

- **Be an ambassador**: Spread the word about rsyslog and how to best use it.
- **Offer support**:
  - On GitHub: [rsyslog repository](https://github.com/rsyslog/rsyslog)
  - On the mailing list: [mailing list signup](http://lists.adiscon.net/mailman/listinfo/rsyslog)
- **Improve documentation**:
  - Contribute to [rsyslog-doc](https://github.com/rsyslog/rsyslog-doc)
  - Help maintain [rsyslog.com/doc](http://rsyslog.com/doc)
- **Maintain infrastructure**: Help with project websites, CI, or packaging.
- **Develop code**: Implement new features, improve existing ones, or fix bugs.
- AI-assisted contributions should follow [AGENTS.md](AGENTS.md) for setup and workflow guidance.

---

## Pull Requests

### Drafts and Experiments
- You may submit early PRs for CI testing. Mark them clearly as `WiP:`.
- For experiments, use **draft pull requests**. Close them after testing.

### Target Branch
- All PRs must target the `master` branch (not `main`).
- **Why `master`?** The `master` name is deeply integrated into our test and deployment infrastructure, across many legacy environments. Renaming would disrupt a wide ecosystem of automated tools and integrations.

---

## AI-Based Code Review (Experimental)

We are currently testing AI-based code review for pull requests. At this time, we use **Google Gemini** to automatically analyze code and provide comments on new PRs.

- These reviews are **informational only**.
- Every contribution is still **manually reviewed** by human experts.
- The goal is to evaluate how AI can support contributor feedback and code quality assurance.

Please report any issues, false positives, or suggestions about the AI review process.

---

## Commit Guidelines for AI Agents

If you use an AI agent (e.g. GitHub Copilot, ChatGPT, Codex), include a commit footer tag:

```
AI-Agent: Codex 2025-06
```

This helps us track and evaluate contributions and agent capabilities.

---

## Patch Requirements

All submissions must:

- Use RainerScript (no legacy `$...` config statements).
- Compile cleanly with **no warnings** on both `gcc` and `clang`.
- Pass `clang` static analysis with no findings.
- Pass **all CI test jobs** (see below).
- Include:
  - Tests (for new features or bugfixes)
  - Documentation updates (for new features)
  - Squashed commits ([why it matters](https://rainer.gerhards.net/2019/03/squash-your-pull-requests.html))

### Testbench Tips

- Write small, focused tests.
- Use similar existing tests as templates.
- Test driver is `make check`, backed by `tests/diag.sh`.
- Test concurrency is limited due to resource load.

### Compiler Warnings

False positives must be resolved, not ignored. Only in extreme cases use:

```c
#pragma diagnostic push
#pragma GCC diagnostic ignored "..."
// ... function
#pragma diagnostic pop
```

Apply to **single functions only**.

See: [Why static analysis matters](https://rainer.gerhards.net/2018/06/why-static-code-analysis.html)

---

## Continuous Integration (CI)

All PRs are tested via:
- GitHub Actions workflows (including multi-platform docker-based tests)
- Additional off-GitHub runners (Solaris, exotic distros)

Build durations: 10–40 min per matrix job.
Some known flaky tests exist; we re-run these manually if needed.

---

## GDPR and Privacy

By contributing, your name, email, commit hash, and timestamp become part of the git history.
This is fundamental to git and public open source contribution. If you have privacy concerns, you may commit anonymously:

```bash
git commit --author "anonymous <gdpr@example.com>"
```

Avoid `--gpg-sign` in that case.

If you use your real identity, note:
- We cannot delete commits retroactively without damaging project history.
- Identity data is used only to maintain copyright tracking and audit trails.
- Public commits may be copied by third parties and redistributed without control.
