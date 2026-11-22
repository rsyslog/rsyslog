# Rsyslog – What Is It?

**Rsyslog** is a **r**ocket-fast **sys**tem for **log** processing pipelines.

It offers high performance, advanced security features, and a **modular microkernel-like architecture**.
Originally a regular syslogd, rsyslog has evolved into a highly versatile logging solution capable of ingesting data from numerous sources, transforming it, and outputting it to a wide variety of destinations.

Rsyslog can deliver over one million messages per second to local destinations under minimal processing (based on v7, Dec 2013). Even with complex routing and remote forwarding, performance remains excellent.

---

## Table of Contents
- [Getting Rsyslog News](#getting-rsyslog-news)
- [🤖 Rsyslog Assistant (Experimental AI Help)](#rsyslog-assistant-experimental-ai-help)
- [Getting Help (Other Sources)](#getting-help-other-sources)
- [Installation](#installation)
- [Contributing](#contributing)
- [AI-Based Code Review (Experimental)](#ai-based-code-review-experimental)
- [Documentation](#documentation)
- [Project Philosophy](#project-philosophy)
- [Global Accessibility and Collaboration](#global-accessibility-and-collaboration)
- [Sponsors](#sponsors)
- [Legal Notice (GDPR)](#legal-notice-gdpr)

---

## Getting Rsyslog News

Stay up to date with official rsyslog announcements and community insights:

**Official Channels**
* [Website](https://rsyslog.com/) – official news and documentation
* [RSS Feed](https://rsyslog.com/feed/)
* [Telegram](https://t.me/rsyslog_official)
* [WhatsApp](https://whatsapp.com/channel/0029VbBJQLhCxoArVHjrL32E)

**Maintainer Insights**
Updates, technical commentary, and behind-the-scenes notes from
[Rainer Gerhards](https://www.linkedin.com/in/rgerhards/), rsyslog founder and maintainer:
* [LinkedIn](https://www.linkedin.com/in/rgerhards/)
* [X (Twitter)](https://x.com/rgerhards)
* [Blog – rainer-gerhards.net](https://rainer.gerhards.net/)

---

## 🤖 Rsyslog Assistant (Experimental AI Help)

Need help with rsyslog configuration or troubleshooting?
Try the **[rsyslog Assistant](https://rsyslog.ai)** — your AI-powered support tool built by the rsyslog team.

> ⚠️ *Experimental.* May occasionally generate incorrect config examples — always review before applying.

✅ Trained on official docs and changelogs
✅ Covers both Linux rsyslog and Windows Agent
✅ Version-aware and best-practice focused

👉 Try it now: [rsyslog.ai](https://rsyslog.ai)

---

## Getting Help (Other Sources)

* **💬 GitHub Discussions:** [Ask questions or start a conversation](https://github.com/rsyslog/rsyslog/discussions)
* **📧 Mailing List:** [rsyslog mailing list](https://lists.adiscon.net/mailman/listinfo/rsyslog)
* **🐛 GitHub Issues:** [Open an issue](https://github.com/rsyslog/rsyslog/issues)

---

## Installation

### Via Distribution Package Managers
Rsyslog is available in most Linux distribution repositories and often is pre-installed.

### Project-Provided Packages (Latest Versions)
Distributions may lag behind in packaging the latest rsyslog releases.
Official builds for newer versions are available here:

* [RPM-based systems](https://rsyslog.com/rhelcentos-rpms/)
* [Ubuntu](https://rsyslog.com/ubuntu-repository/)
* [Debian](https://rsyslog.com/debian-repository/)
* [Official Containers](packaging/docker/README.md)

For users in regions where GitHub access is limited, see the
[Global Accessibility and Collaboration](#global-accessibility-and-collaboration) section for alternative mirrors.

<details>
<summary><strong>Building from Source (click to expand)</strong></summary>

See: [Build Instructions](https://rsyslog.com/doc/v8-stable/installation/build_from_repo.html)

#### Build Environment Requirements
* `pkg-config`
* `libestr`
* `liblogging` (stdlog component, for testbench)

Build support libraries from source if you're working with the latest git master.

#### Branch Guidance
The `master` branch tracks active development.
For production use, prefer the latest tagged release.

#### OS-Specific Build Instructions
Refer to the respective section in the original README for required packages on CentOS, Ubuntu, Debian, SUSE, etc.

#### Development Containers & Testing
Ready-to-use build environments are provided in `packaging/docker/dev_env`.
These images were previously built in the separate [rsyslog-docker](https://github.com/rsyslog/rsyslog-docker) repository and are now maintained here.
See `packaging/docker/README.md` for details.

Runtime container definitions are in `packaging/docker/rsyslog`.
Run the test suite inside the container with:

```bash
make check -j4
```

</details>

---

## Contributing

Rsyslog is a community-driven open-source project. Contributions are welcome and encouraged!

* See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines
* Starter tasks: [Good First Issues](https://rsyslog.com/tool_good-first-issues)
* To develop new output plugins in Python or Perl, see [plugins/external/README.md](plugins/external/README.md)
* If you're working with AI coding agents (e.g. GitHub Copilot, OpenAI Codex), see [AGENTS.md](AGENTS.md)
* Community: [Code of Conduct](CODE_OF_CONDUCT.md)

If GitHub access is difficult in your region, please contact us or use one of the mirrors listed below;
we will help route your patch for review.

**Commit Assistant (recommended):**
Draft compliant commit messages with
[rsyslog Commit Assistant](https://rsyslog.com/tool_rsyslog-commit-assistant)
and follow rules in [CONTRIBUTING.md](CONTRIBUTING.md).
Put the substance into the **commit message** (amend before PR if needed).

---

### AI-Based Code Review (Experimental)

We are currently testing AI-based code review for pull requests.
We use AI review tools, including **Google Gemini** and **Enclave**, to support pull request review.

[![Reviewed by Enclave](https://img.shields.io/badge/reviewed%20by-Enclave-111111?style=flat-square&labelColor=111111&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB3aWR0aD0iMTI2IiBoZWlnaHQ9IjEyNiIgdmlld0JveD0iMTYgMTggMTI2IDEyNiIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48ZyBzdHJva2U9Im5vbmUiIGZpbGw9Im5vbmUiIGZpbGwtcnVsZT0ibm9uemVybyI%2BPGcgdHJhbnNmb3JtPSJ0cmFuc2xhdGUoLTE2LCAtMjIpIj48cGF0aCBkPSJNMTMzLjcsNDQuMSBDMTM3LjQsNDYuNiAxMzguNCw1MS41IDEzNi4wLDU1LjIgQzEyNy4yLDY4LjYgMTEyLjMsNzYuOCA5Niw3Ni44IEM3OS42LDc2LjggNjQuNyw2OC42IDU1LjksNTUuMiBDNTMuNSw1MS41IDU0LjUsNDYuNiA1OC4yLDQ0LjEgQzYxLjksNDEuNyA2Ni44LDQyLjcgNjkuMyw0Ni40IEM3NS4xLDU1LjQgODUuMSw2MC45IDk2LDYwLjkgQzEwNi44LDYwLjkgMTE2LjgsNTUuNCAxMjIuNiw0Ni40IEMxMjUuMSw0Mi43IDEzMC4wLDQxLjcgMTMzLjcsNDQuMSBaIiBmaWxsPSIjZmZmIj48L3BhdGg%2BPHBhdGggZD0iTTEzNS42LDU1LjggQzEyOC45LDY0LjIgMTI2LjgsNzUuNCAxMzAuMSw4NS44IEMxMzMuNSw5Ni4wIDE0MS41LDEwMy43IDE1MS42LDEwNi42IEwxNTIuMSwxMDYuOCBDMTU2LjMsMTA3LjkgMTU4LjgsMTEyLjMgMTU3LjcsMTE2LjYgQzE1Ni41LDEyMC45IDE1Mi4xLDEyMy4zIDE0Ny44LDEyMi4yIEMxMzIuNCwxMTcuOSAxMjAuMCwxMDYuMiAxMTQuOSw5MC43IEMxMDkuOSw3NS4xIDExMy4xLDU4LjQgMTIzLjEsNDUuOSBDMTI1LjgsNDIuNCAxMzAuOCw0MS44IDEzNC4zLDQ0LjYgQzEzNy43LDQ3LjMgMTM4LjMsNTIuMSAxMzUuNyw1NS42IEwxMzUuNiw1NS44IFoiIGZpbGw9IiNmZmYiPjwvcGF0aD48cGF0aCBkPSJNOTYuMywxNjEuOCBDOTEuOSwxNjIuMCA4OC4yLDE1OC42IDg4LjAsMTU0LjIgQzg3LjMsMTM4LjIgOTQuNSwxMjIuNyAxMDcuNywxMTMuMSBDMTIwLjksMTAzLjUgMTM3LjgsMTAxLjQgMTUyLjgsMTA3LjAgQzE1Ni45LDEwOC42IDE1OS4wLDExMy4yIDE1Ny40LDExNy4zIEMxNTUuOSwxMjEuMyAxNTEuNCwxMjMuNCAxNDcuNCwxMjIuMCBMMTQ3LjEsMTIyLjAgQzEzNy4xLDExOC4yIDEyNS45LDExOS42IDExNy4xLDEyNi4wIEMxMDguNCwxMzIuMyAxMDMuNiwxNDIuNCAxMDMuOSwxNTMuMCBMMTAzLjksMTUzLjUgQzEwNC4xLDE1Ny45IDEwMC43LDE2MS42IDk2LjMsMTYxLjggWiIgZmlsbD0iI2ZmZiI%2BPC9wYXRoPjxwYXRoIGQ9Ik04OC4wLDE1My41IEM4OC40LDE0Mi43IDgzLjYsMTMyLjQgNzQuOCwxMjYuMCBDNjYuMiwxMTkuNyA1NS4xLDExOC4yIDQ1LjIsMTIxLjggTDQ0LjgsMTIyLjAgQzQwLjYsMTIzLjUgMzYuMCwxMjEuNCAzNC41LDExNy4zIEMzMi45LDExMy4yIDM1LjAsMTA4LjYgMzkuMSwxMDcuMCBDNTQuMSwxMDEuNCA3MS4wLDEwMy41IDg0LjIsMTEzLjEgQzk3LjQsMTIyLjcgMTA0LjYsMTM4LjIgMTAzLjksMTU0LjIgQzEwMy43LDE1OC42IDEwMC4wLDE2Mi4wIDk1LjYsMTYxLjggQzkxLjMsMTYxLjYgODcuOSwxNTguMCA4OC4wLDE1My43IEw4OC4wLDE1My41IFoiIGZpbGw9IiNmZmYiPjwvcGF0aD48cGF0aCBkPSJNNTcuNiw0NC42IEM2MS4xLDQxLjggNjYuMSw0Mi40IDY4LjgsNDUuOSBDNzguOCw1OC40IDgyLjAsNzUuMSA3Ny4wLDkwLjcgQzcxLjksMTA2LjIgNTkuNSwxMTcuOSA0NC4xLDEyMi4yIEMzOS44LDEyMy4zIDM1LjQsMTIwLjkgMzQuMiwxMTYuNiBDMzMuMSwxMTIuNCAzNS41LDEwOC4xIDM5LjYsMTA2LjggTDM5LjgsMTA2LjggQzUwLjEsMTAzLjkgNTguNCw5Ni4xIDYxLjgsODUuOCBDNjUuMSw3NS41IDYzLjEsNjQuNSA1Ni42LDU2LjIgTDU2LjMsNTUuOCBDNTMuNiw1Mi4zIDU0LjEsNDcuMyA1Ny42LDQ0LjYgWiIgZmlsbD0iI2ZmZiI%2BPC9wYXRoPjwvZz48L2c%2BPC9zdmc%2B)](https://enclave.ai/?utm_source=github&utm_medium=readme_badge)

* Reviews are **informational only**
* Every contribution is still **manually reviewed** by human experts
* The goal is to evaluate how AI can support contributor feedback and code quality assurance

Please report any issues, false positives, or suggestions about the AI review process.

---

## Documentation

Documentation is located in the `doc/` directory of this repository.
Contributions to the documentation should be made there.

Visit the latest version online:
* [docs.rsyslog.com/doc](https://docs.rsyslog.com/doc/)

---

## Project Philosophy

Rsyslog development is driven by real-world use cases, open standards, and an active community.
While sponsored primarily by Adiscon, technical decisions are made independently via consensus.

All contributors are welcome — there is no formal membership beyond participation.

---

## Global Accessibility and Collaboration

Rsyslog aims to remain accessible worldwide. Different regions sometimes face network or platform limits, so we provide mirrors to ensure that everyone can obtain the source code and contribute on equal terms.

- **GitHub (canonical):** https://github.com/rsyslog/rsyslog
- **Community mirror (China, maintained by Gitee):** https://gitee.com/mirrors_rsyslog

All pull requests ultimately flow through GitHub, where our CI and review infrastructure run.
If you experience access issues but can provide a patch, the rsyslog team will gladly assist in forwarding it to GitHub.
Our contribution workflow remains GitHub-based for now because CI and automated testing are tightly integrated there.
Always verify releases and tags from the official GitHub repository before production use.

> *Open source should have no borders.*
> (FR) Le code ouvert ne devrait pas avoir de frontières
> (ZH) 开源应该没有边界
> (JA) オープンソースに国境はない
> (ES) El código abierto no debe tener fronteras
> (HI) ओपन सोर्स की कोई सीमाएँ नहीं होनी चाहिए
> (AR) المصدر المفتوح يجب ألا يكون له حدود

---

## Sponsors

The rsyslog project is proudly supported by organizations that help sustain its continuous development, infrastructure, and innovation.
(See also [Project Philosophy](#project-philosophy).)

### Prime Sponsor

**[Adiscon GmbH](https://www.adiscon.com/)**
Adiscon employs core rsyslog developers including Rainer Gerhards and provides ongoing engineering, infrastructure, and CI resources.
Through its commercial Windows log management products — such as [WinSyslog](https://www.winsyslog.com/) and [Rsyslog Windows Agent](https://www.winsyslog.com/rsyslog-windows-agent/) — Adiscon helps fund rsyslog’s continued open-source development and ecosystem growth.

### Additional Acknowledgments

rsyslog also benefits from various open-source infrastructure providers and community initiatives that make modern CI, code hosting, and collaboration possible.

---

_If your organization benefits from rsyslog and would like to contribute to its sustainability, please consider [sponsoring or contributing](https://github.com/sponsors/rsyslog)._

---

## Legal Notice (GDPR)

Contributions to rsyslog are stored in git history and publicly distributed.
