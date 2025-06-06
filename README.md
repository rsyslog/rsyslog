# Rsyslog - What is it?

[![Help Contribute to Open Source](https://www.codetriage.com/rsyslog/rsyslog/badges/users.svg)](https://www.codetriage.com/rsyslog/rsyslog)

**Rsyslog** is a **r**ocket-fast **sys**tem for **log** processing.

It offers high-performance, advanced security features, and a modular design.
Originally a regular syslogd, rsyslog has evolved into a highly versatile logging solution capable of ingesting data from numerous sources, transforming it, and outputting it to a wide variety of destinations.

Rsyslog can deliver over one million messages per second to local destinations under minimal processing (based on v7, Dec 2013). Even with complex routing and remote forwarding, performance remains excellent.

---

## Getting Help
- **Mailing List:** [rsyslog mailing list](https://lists.adiscon.net/mailman/listinfo/rsyslog)
- **GitHub Issues:** [Open an issue](https://github.com/rsyslog/rsyslog/issues)

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
For a ready-to-use environment, use the images from [rsyslog-docker](https://github.com/rsyslog/rsyslog-docker). They contain all build dependencies. Run the test suite with `make check` (limit to `-j4`).

---

## Contributing
Rsyslog is a community-driven open-source project. Contributions are welcome and encouraged!

- See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed contribution guidelines.
- To develop new output plugins in Python or Perl, see: [plugins/external/README.md](plugins/external/README.md)

If you're working with AI coding agents (e.g. GitHub Copilot, OpenAI Codex), note that we support these workflows with agent-specific instructions in `AGENTS.md`.

---

## Documentation
The complete and current documentation is maintained in the separate [`rsyslog-doc`](https://github.com/rsyslog/rsyslog-doc) project.

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
Contributions to rsyslog are stored in git history and publicly distributed. Please refer to `CONTRIBUTING.md` for detailed GDPR-related information.
