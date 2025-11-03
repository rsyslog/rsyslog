# Rsyslog – What Is It?

**Rsyslog** is a **r**ocket-fast **sys**tem for **log** processing pipelines.

It offers high performance, advanced security features, and a modular design.  
Originally a regular syslogd, rsyslog has evolved into a highly versatile logging solution capable of ingesting data from numerous sources, transforming it, and outputting it to a wide variety of destinations.

Rsyslog can deliver over one million messages per second to local destinations under minimal processing (based on v7, Dec 2013). Even with complex routing and remote forwarding, performance remains excellent.

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

**Commit Assistant (recommended):**  
Draft compliant commit messages with  
[rsyslog Commit Assistant](https://rsyslog.com/tool_rsyslog-commit-assistant)  
and follow rules in [CONTRIBUTING.md](CONTRIBUTING.md).  
Put the substance into the **commit message** (amend before PR if needed).

---

### AI-Based Code Review (Experimental)

We are currently testing AI-based code review for pull requests.  
At this time, we use **Google Gemini** to automatically analyze code and provide comments on new PRs.

* Reviews are **informational only**  
* Every contribution is still **manually reviewed** by human experts  
* The goal is to evaluate how AI can support contributor feedback and code quality assurance

Please report any issues, false positives, or suggestions about the AI review process.

---

## Documentation

Documentation is located in the `doc/` directory of this repository.  
Contributions to the documentation should be made there.

Visit the latest version online:  
* [rsyslog.com/doc](https://rsyslog.com/doc/)

---

## Project Philosophy

Rsyslog development is driven by real-world use cases, open standards, and an active community.  
While sponsored primarily by Adiscon, technical decisions are made independently via consensus.

All contributors are welcome — there is no formal membership beyond participation.

---

## Sponsors

The rsyslog project is proudly supported by organizations that help sustain its continuous development, infrastructure, and innovation.  
(See also [Project Philosophy](#project-philosophy).)

### Prime Sponsor

**[Adiscon GmbH](https://www.adiscon.com/)**  
Adiscon employs core rsyslog developers including Rainer Gerhards and provides ongoing engineering, infrastructure, and CI resources.  
Through its commercial Windows log management products — such as [WinSyslog](https://www.winsyslog.com/) and [Rsyslog Windows Agent](https://www.winsyslog.com/rsyslog-windows-agent/) — Adiscon helps fund rsyslog’s continued open-source development and ecosystem growth.

### Major Sponsor

<p>
  <a href="https://www.digitalocean.com/">
    <img src="https://opensource.nyc3.cdn.digitaloceanspaces.com/attribution/assets/SVG/DO_Logo_horizontal_blue.svg" width="201px" alt="DigitalOcean Logo">
  </a>
</p>

**[DigitalOcean](https://www.digitalocean.com/)** powers key parts of rsyslog’s CI pipeline, package distribution network, and AI infrastructure as part of their [#DOforOpenSource](https://www.digitalocean.com/open-source) initiative.  
Their support enables fast, globally available builds and the next generation of AI-assisted rsyslog documentation and tooling.

### Additional Acknowledgments

rsyslog also benefits from various open-source infrastructure providers and community initiatives that make modern CI, code hosting, and collaboration possible.

---

_If your organization benefits from rsyslog and would like to contribute to its sustainability, please consider [sponsoring or contributing](https://github.com/sponsors/rsyslog)._  

---

## Legal Notice (GDPR)

Contributions to rsyslog are stored in git history and publicly distributed.
