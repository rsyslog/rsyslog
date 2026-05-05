This directory contains tools for use by development and
CI. The goal is to have a consistent way to do things.

## Scripts

- run-static-analyzer.sh
  Run the currently approved rsyslog project clang static analyzer.

- codex-setup.sh
  Prepare an Ubuntu 24.04 WSL checkout for full rsyslog development. It
  installs the packages and helper libraries used by the Ubuntu 24.04
  development container and can optionally verify a build with the container's
  RSYSLOG_CONFIGURE_OPTIONS.

- codex-setup-wsl.ps1
  Windows wrapper for running codex-setup.sh inside a WSL distro.

## Environment Variables

- RSYSLOG\_HOME
  root of rsyslog sources - e.g. unzipped tarball, git checkout

## Files

### default\_dev\_container

Contains the full name of the container to be used for development
task if no other is given.

### codex-wsl-bootstrap-prompt.md

Copy-paste prompt for asking Codex to set up rsyslog development on a new Windows machine.
