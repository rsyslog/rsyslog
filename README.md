# Rsyslog - What is it?

[![Help Contribute to Open Source](https://www.codetriage.com/rsyslog/rsyslog/badges/users.svg)](https://www.codetriage.com/rsyslog/rsyslog)

**Rsyslog** is a **r**ocket-fast **sys**tem for **log** processing.

It offers high-performance, advanced security features, and a modular design.
Originally a regular syslogd, rsyslog has evolved into a highly versatile logging solution capable of ingesting data from numerous sources, transforming it, and outputting it to a wide variety of destinations.

Rsyslog can deliver over one million messages per second to local destinations under minimal processing (based on v7, Dec 2013). Even with complex routing and remote forwarding, performance remains excellent.

---

## getting rsyslog news

* [www.rsyslog.com](https://www.rsyslog.com/), via [RSS](https://www.rsyslog.com/feed/)
* Messenger Platforms [Telegram](https://t.me/rsyslog_official), [WhatsApp](https://whatsapp.com/channel/0029VbBJQLhCxoArVHjrL32E) - operated by maintainer Rainer Gerhards
---

## 🤖 rsyslog Assistant (Experimental AI Help)

Need help with rsyslog configuration or troubleshooting?
Try the **[rsyslog Assistant](https://chatgpt.com/g/g-686f63c947688191abcbdd8d5d494626-rsyslog-assistant)** — your AI-powered support tool, built by the rsyslog team.

> ⚠️ *Experimental.* May occasionally generate incorrect config examples — always review before applying.

✅ Trained on official docs and changelogs
✅ Covers both Linux rsyslog and Windows Agent
✅ Version-aware and best-practice focused

👉 Try it now: [chatgpt.com/g/g-686f63c947688191abcbdd8d5d494626-rsyslog-assistant](https://chatgpt.com/g/g-686f63c947688191abcbdd8d5d494626-rsyslog-assistant)


## Getting Help (Other Sources)

* **💬 GitHub Discussions:** [Ask questions or start a conversation](https://github.com/rsyslog/rsyslog/discussions)
* **📧 Mailing List:** [rsyslog mailing list](https://lists.adiscon.net/mailman/listinfo/rsyslog)
* **🐛 GitHub Issues:** [Open an issue](https://github.com/rsyslog/rsyslog/issues)

---

## Installation

### Via Distribution Package Managers
Rsyslog is available in the package repositories of most Linux distributions. On non-systemd systems (e.g., Ubuntu), rsyslog is often pre-installed.

### Project-Provided Packages (for latest versions)
Distributions often lag behind in packaging the latest rsyslog releases. Official builds for newer versions are available here:

- [RPM-based systems](https://www.rsyslog.com/rhelcentos-rpms/)
- [Ubuntu](https://www.rsyslog.com/ubuntu-repository/)
- [Debian](https://www.rsyslog.com/debian-repository/)

### Building from Source
See: [Build Instructions](https://www.rsyslog.com/doc/v8-stable/installation/build_from_repo.html)

#### Build Environment Requirements
- `pkg-config`
- `libestr`
- `liblogging` (stdlog component, for testbench)

Build support libraries from source if you're working with the latest git master.

#### OS-specific Build Instructions
Refer to the respective section in the original README for required packages on CentOS, Ubuntu, Debian, SUSE, etc.

#### Development Containers & Testing
Ready-to-use build environments are provided in `packaging/docker/dev_env`. These images were previously built in the separate [rsyslog-docker](https://github.com/rsyslog/rsyslog-docker) repository and are now maintained here. See `packaging/docker/README.md` for details. Runtime container definitions are in `packaging/docker/rsyslog`. Run the test suite inside the container with `make check` (limit to `-j4`).

---

## Contributing
Rsyslog is a community-driven open-source project. Contributions are welcome and encouraged!

- See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed contribution guidelines.
- Starter tasks: https://www.rsyslog.com/tool_good-first-issues
- To develop new output plugins in Python or Perl, see: [plugins/external/README.md](plugins/external/README.md)
- If you're working with AI coding agents (e.g. GitHub Copilot, OpenAI Codex), note that we support these workflows with agent-specific instructions in [AGENTS.md](AGENTS.md).
- Community: [Code of Conduct](CODE_OF_CONDUCT.md)

**Commit Assistant (recommended):** Draft compliant commit messages with
https://www.rsyslog.com/tool_rsyslog-commit-assistant (see rules in
[CONTRIBUTING.md](CONTRIBUTING.md)). Put the substance into the **commit
message** (amend before PR if needed).

---

### AI-Based Code Review (Experimental)

We are currently testing AI-based code review for pull requests. At this time, we use **Google Gemini** to automatically analyze code and provide comments on new PRs.

- These reviews are **informational only**.
- Every contribution is still **manually reviewed** by human experts.
- The goal is to evaluate how AI can support contributor feedback and code quality assurance.

Please report any issues, false positives, or suggestions about the AI review process.

---

## Documentation
Documentation is located in the `doc/` directory of this repository. Contributions to the documentation should be made there.

Visit the latest version online:
- [rsyslog.com/doc](https://www.rsyslog.com/doc/)

---

## Project Philosophy
Rsyslog development is driven by real-world use cases, open standards, and an active community. While sponsored primarily by Adiscon, technical decisions are made independently via mailing list consensus.

All contributors are welcome—there is no formal membership beyond participation.

---

## Project Funding
Adiscon GmbH supports rsyslog through:
- Custom development services
- Professional support contracts

Third-party contributions, services, and integrations are welcome.

---

## Legal Notice (GDPR)
Contributions to rsyslog are stored in git history and publicly distributed.
