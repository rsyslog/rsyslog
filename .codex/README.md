# Codex Repo Setup

This repository ships a repo-local Codex hook configuration for trusted projects.

## What it does

- Enables Codex hooks for this repository via `.codex/config.toml`
- Runs `.codex/pre_commit_format_gate.sh` before Bash-based `git commit` commands
- Understands both direct `git commit ...` invocations and shell-wrapped forms such as `bash -lc 'git commit ...'`
- Runs the deterministic repo-policy focus checks first against the staged commit snapshot
- If those invariants fail, the hook exits early and prints the specific failures for the agent to fix
- The hook runs `./devtools/format-code.sh --git-changed`
- If `clang-format-18` is not installed, the hook allows the commit without blocking
- If no tracked `.c` or `.h` files have changed, the hook skips formatting work
- If formatting fails, the `git commit` tool call is blocked and Codex is told to fix formatting first
- If formatting updates `.c` or `.h` files, the hook stages those tracked formatter updates automatically and then allows the commit
- If partially staged `.c` or `.h` files are present, the hook blocks because auto-restaging would not be safe

## Requirements

- The repository must be trusted so Codex loads `.codex/config.toml`
- Codex hooks must be available in your Codex build

## Scope

This affects Codex users working in this repository. It does not replace normal project review or CI checks, and it does not affect contributors who are not using Codex.
