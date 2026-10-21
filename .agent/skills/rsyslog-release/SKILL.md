---
name: rsyslog-release
description: Prepare and publish scheduled-stable rsyslog releases, then hand main off to the next release cycle. Use for release metadata, docs and source artifacts, signed tags, GitHub release announcements, stable-to-main merges, active milestone rotation, and OBS release packaging.
---

# Rsyslog Release

Use this skill for the complete scheduled-stable `.0` release workflow. Use
`rsyslog_v8stable_patch_flow` for post-release `.1`, `.2`, or later patch work.
Use `rsyslog_changelog`, `rsyslog_doc`, and `rsyslog_commit` alongside this
skill when their narrower rules apply.

## Release variables

Confirm these values before editing anything:

- `STABLE_VERSION`: `8.yymm.0`, for example `8.2610.0`
- `RELEASE_DATE`: the actual scheduled release date
- `NEXT_VERSION`: the next bi-monthly cycle, for example `8.2612.0`
- `NEXT_DATE`: the next scheduled release date
- `ACTIVE_MILESTONE`: normally `v8.yymm`, for example `v8.2612`

Do not infer the next cycle from a stale generated file. Check the current
release schedule, existing ChangeLog sections, and GitHub milestones.

## 1. Prepare the stable release

1. Read `AGENTS.local.md` if present. For package archives, read
   `.github/DAILY_STABLE_PACKAGE_ARCHIVE.md` before building or publishing.
2. Confirm the starting branch is `v8-stable`, fetch the current remote refs,
   and preserve unrelated worktree changes. Do not rewrite released history.
3. Update only the release metadata needed for the stable snapshot:
   - top `ChangeLog` scheduled-release header with `STABLE_VERSION` and
     `RELEASE_DATE`;
   - `configure.ac` package version and `VERSION_MONTH`;
   - `doc/source/conf.py` scheduled-stable `version` and `release` values.
4. Build the rendered documentation with the repository docs command when
   docs metadata or rendered documentation is involved. Treat existing
   nonfatal warnings as evidence to report, not as silent failures.
5. Run `make dist` and validate the source archive:
   - `gzip -t` and `tar -tzf` succeed;
   - `.tarball-version`, configure metadata, and ChangeLog identify the
     stable release;
   - the archive has a SHA256 checksum and no unexpected generated files.

Generated files such as `configure` may be ignored by Git and stale in a
developer checkout. Do not commit them merely to make the worktree look
current; regenerate them when a local build needs them. If a locally built
archive differs from the GitHub Actions archive, compare file lists and
embedded versions before deciding whether the difference is harmless.

6. Create a signed release-preparation commit and verify its signature. Create
   a signed annotated tag named `vSTABLE_VERSION` with the annotation format
   used by recent releases, normally `scheduled stable release vSTABLE_VERSION`.
7. Push the stable branch and tag only with explicit authorization. If the
   repository's local validation exception is used, record the exact skipped
   gate in the handoff.

## 2. Verify and publish GitHub release output

After the tag workflow completes:

- verify the workflow conclusion is successful;
- verify the draft release has the expected tag, source tarball, and `.sha256`
  asset;
- verify the published asset passes its uploaded checksum;
- compare the release body with recent scheduled-stable announcements;
- include notable changes, compatibility notes, overall hardening, and
  security/robustness fixes accumulated from daily-stable since the previous
  scheduled stable release;
- keep contributor attribution accurate, naming external contributors and
  using the repository's established Adiscon attribution convention;
- publish the release only when its body and assets are ready.

Create a condensed GitHub Discussion in the `Announcements` category after
publication. Put the direct release link in the first paragraph and include
short highlights, compatibility notes, hardening/security maintenance, and a
brief contributor/community thanks. Do not create a local Markdown file just
to hold the announcement unless the user explicitly requests one.

## 3. Merge stable history back to main

After the stable release is published:

1. Refresh `main` from the remote.
2. Merge `origin/v8-stable` into `main` with `--no-ff` so the stable history
   remains reachable and future merges stay clean.
3. Verify `origin/v8-stable` is an ancestor of `main` and push the merge.
4. Do not regenerate the already-published stable tag or source assets merely
   because main is moving to the next cycle.

## 4. Start the next cycle on main

On `main`, make a separate signed commit that:

- adds a pending `NEXT_VERSION` ChangeLog header dated `NEXT_DATE`;
- changes `configure.ac` to `NEXT_VERSION.daily` and updates
  `AC_DEFINE(VERSION_MONTH, ...)`;
- changes docs to `version = '8.yymm'`, comments the stable `release` value,
  and enables `release = version + ' daily stable'`;
- leaves historical stable-release entries unchanged.

Then build the docs, run `git diff --check`, verify the signed commit, and push
`main` with the authorized validation policy.

## 5. Rotate the active PR milestone

Do not assume a repository file exists. Search tracked and ignored repository
configuration, Git history, and GitHub workflow files for a codified milestone
rule. If no such rule exists, treat the GitHub milestone as operational state:

- confirm the open milestone for `NEXT_VERSION` (normally titled `v8.yymm`);
- inspect PRs opened after the release and move human release-work PRs still
  carrying the closed prior milestone to the active one;
- do not blanket-reassign Dependabot or other automated maintenance PRs unless
  repository policy says to do so;
- report any missing default-assignment automation instead of inventing a new
  one during release publication.

## 6. Stage and promote OBS release packages

Start after the GitHub release assets are available; this can run alongside
the main-branch handoff. Keep the source release and package-publication
statuses separate when manual package actions are still pending.

1. Always review distro/version and architecture coverage for the release.
   Compare vendor support status with the actual OBS native baselines, propose
   additions/removals, and preserve explicit maintainer overrides. Skip versions
   OBS does not genuinely support; a project name alone is insufficient if its
   download configuration still points at a different release. Test approved
   coverage changes in staging first; changing official coverage is a separate
   promotion decision.
2. Use `home:rainergerhards:branches:home:rgerhards` as the reusable staging
   project and `home:rgerhards` as the official destination. Read local guidance
   for separate OSC credential profiles and verify the authenticated identity
   before mutations. Never print credentials or replace one account's profile
   with the other account's settings.
3. Download the published GitHub source tarball and its checksum asset; verify
   the checksum and embedded release version. Update staging RPM specs and
   changelogs plus the active Debian `.dsc` files and packaging archives.
   Keep `.dsc` build dependencies consistent with `debian/control`, increment
   packaging revisions when changing already-published packages, and regenerate
   archive filenames, sizes, and checksums. Preserve the upstream release asset.
4. Commit to staging and babysit the complete enabled repository/architecture
   matrix, including the dependency libraries, not just one RPM and one DEB.
   Inspect actual logs before fixing failures; resolve native dependency names
   before dropping a supported feature. Record intentional feature omissions
   and test skips. Follow dependency waits, native-baseline-triggered rebuilds,
   signing, and result processing through to their final states. Do not require
   the maintainer to report routine failures before investigating them.
5. Submit the tested source revision to the official project when authorized.
   Always use `osc submitrequest --no-cleanup --revision "$SRCMD5"` with the
   tested source checksum and verify the request XML has
   `<sourceupdate>update</sourceupdate>`, not `cleanup`. Never delete the staging
   fork on acceptance: it is reused for subsequent changes. Record the request
   URL and verify its source, destination, and revision through the API.
6. Accept only with maintainer authorization, then check official builds and
   publication. Reconcile staging with accepted official sources while keeping
   unpromoted staging changes. Verify public repository signatures and install
   and smoke-test the exact published version on matching targets before calling
   those packages operational. Follow the package archive program's validation
   rules; a green build alone does not establish publication or installability.

Keep this release step focused on delivering the release. Unrelated OBS
cleanup, account repair, and packaging enhancements are separate work.

## Completion evidence

A release handoff is complete only when the report includes:

- signed stable commit and tag identifiers;
- docs-build result and any known warnings;
- published release URL, workflow run, asset names, and checksum result;
- announcement Discussion URL;
- stable-to-main merge commit and remote refs;
- signed next-cycle main commit and active milestone status;
- OBS coverage review, staging build matrix, submit-request URL/revision,
  fork-preservation check, and official publication/install verification, or
  the explicit outstanding manual package handoff;
- any skipped container or hosted validation gates.

## Open TODO

After the required manual package-build and package-publication actions have
completed, rebuild the release container images using the package-backed
release flow. Confirm package readiness first; release-tagged containers must
not bypass the Adiscon PPA readiness check.
