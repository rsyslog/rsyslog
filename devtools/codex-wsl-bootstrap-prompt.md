# Codex Prompt: Windows WSL Rsyslog Development Setup

Copy this prompt into a new Codex session on a Windows machine:

```text
Set up this Windows machine for rsyslog development with Codex.

Target:
- Use Ubuntu 24.04 under WSL2.
- Use a WSL-native checkout, not C:\git or any /mnt/c path.
- Clone rsyslog into /home/<linux-user>/rsyslog.
- Avoid CRLF problems: configure WSL Git with core.autocrlf=false,
  core.eol=lf, and core.filemode=true.
- Run the rsyslog setup tooling from the repo:
  ./devtools/codex-setup.sh --verify-build

If Ubuntu 24.04 WSL is not installed:
- Install WSL2 and Ubuntu-24.04 first.
- Start Ubuntu once so the Linux user is created.
- Then continue inside that distro.

If apt update times out on the default Ubuntu mirrors:
- Retry setup with:
  ./devtools/codex-setup.sh --apt-mirror http://de.archive.ubuntu.com/ubuntu/ --verify-build

Success criteria:
- rsyslog checkout lives under /home, not /mnt.
- apt update succeeds.
- setup script completes.
- full rsyslog configure and make succeed with RSYSLOG_CONFIGURE_OPTIONS
  from packaging/docker/dev_env/ubuntu/base/24.04/Dockerfile.
- Do not build from Windows-mounted paths.
```

If WSL and the checkout already exist, run this from Windows PowerShell:

```powershell
.\devtools\codex-setup-wsl.ps1 -Distro Ubuntu-24.04 -VerifyBuild
```
