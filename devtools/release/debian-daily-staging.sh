#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: debian-daily-staging.sh <command> [args...]

Commands:
  version
  stamp-changelog <source-dir> <version>
  build-package <workspace> <source-dir> <dist-tarball> <artifact-dir> <version> <build-log>
  manifest <artifact-dir> <version> <suite> <arch>
  generate-repo <artifact-dir> <repo-dir> <suite> <component>
  verify-repo <repo-url> <suite> <version> <expected-key-fingerprint>
EOF
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

base_version() {
  local script_dir

  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  sed -n 's/AC_INIT(\[rsyslog\],\[\([^]]*\)\].*/\1/p' "$script_dir/../../configure.ac" | sed 's/\.daily$//'
}

cmd_version() {
  local base date short_sha run version

  base="$(base_version)"
  [ -n "$base" ] || die "could not determine base version from configure.ac"

  date="$(date -u +%Y%m%d)"
  git rev-parse --verify HEAD >/dev/null 2>&1 || die "invalid git HEAD commit"
  short_sha="$(git rev-parse --short=12 HEAD)"
  run="${GITHUB_RUN_NUMBER:-0}"
  version="${base}~daily${date}.${run}+git${short_sha}-1adiscon1"

  printf '%s\n' "$version"

  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    {
      printf 'version=%s\n' "$version"
      printf 'upstream_version=%s\n' "${version%%-*}"
      printf 'base_version=%s\n' "$base"
    } >> "$GITHUB_OUTPUT"
  fi
}

cmd_stamp_changelog() {
  local source_dir="$1"
  local version="$2"

  [ -d "$source_dir/debian" ] || die "missing debian directory: $source_dir/debian"

  cd "$source_dir"
  export DEBEMAIL="${DEBEMAIL:-release-bot@adiscon.com}"
  export DEBFULLNAME="${DEBFULLNAME:-Adiscon package maintainers}"
  dch --force-distribution --distribution trixie --newversion "$version" \
    "Automated rsyslog Debian daily staging build."
}

cmd_build_package() {
  local workspace="$1"
  local source_dir="$2"
  local dist_tarball="$3"
  local artifact_dir="$4"
  local version="$5"
  local build_log="$6"
  local upstream_version parent parent_real workspace_real rc

  upstream_version="${version%%-*}"
  parent="$(dirname "$source_dir")"
  parent_real="$(readlink -f "$parent")"
  workspace_real="$(readlink -f "$workspace")"

  [ -f "$workspace/$dist_tarball" ] || die "missing dist tarball: $workspace/$dist_tarball"
  [ -d "$source_dir/debian" ] || die "missing Debian source tree: $source_dir"
  case "$parent_real" in
    /|/tmp|"$workspace_real")
      die "refusing to collect package artifacts from unsafe build parent: $parent_real"
      ;;
  esac

  rm -rf "$artifact_dir"
  mkdir -p "$artifact_dir"
  find "$parent" -maxdepth 1 -type f \
    \( -name '*.deb' -o -name '*.dsc' -o -name '*.changes' -o -name '*.buildinfo' \
       -o -name '*.orig.tar.*' -o -name '*.debian.tar.*' \) \
    -delete

  cp "$workspace/$dist_tarball" "$parent/rsyslog_${upstream_version}.orig.tar.gz"

  cd "$source_dir"
  export DEB_BUILD_OPTIONS="${DEB_BUILD_OPTIONS:-nocheck}"

  set +e
  dpkg-buildpackage -us -uc -j"$(nproc)" 2>&1 | tee "$build_log"
  rc=${PIPESTATUS[0]}
  set -e
  if [ "$rc" -ne 0 ]; then
    return "$rc"
  fi

  if grep -Eq '(^|/)(doc/source|source)/[^:]+:[0-9]+: (CRITICAL|ERROR):|^Sphinx error:' "$build_log"; then
    die "Debian docs build emitted fatal Sphinx/docutils diagnostics"
  fi

  cp "$build_log" "$artifact_dir/build.log"
  find "$parent" -maxdepth 1 -type f \
    \( -name "*_${version}_*.deb" -o -name "*_${version}.dsc" \
       -o -name "*_${version}_*.changes" -o -name "*_${version}_*.buildinfo" \
       -o -name "rsyslog_${upstream_version}.orig.tar.*" \
       -o -name "rsyslog_${version}.debian.tar.*" \) \
    -exec cp -a {} "$artifact_dir/" \;

  find "$artifact_dir" -maxdepth 1 -type f -name '*.deb' | grep -q . || die "no .deb artifacts collected"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.dsc' | grep -q . || die "no .dsc artifact collected"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.changes' | grep -q . || die "no .changes artifact collected"
}

cmd_manifest() {
  local artifact_dir="$1"
  local version="$2"
  local suite="$3"
  local arch="$4"

  [ -d "$artifact_dir" ] || die "missing artifact directory: $artifact_dir"

  (
    cd "$artifact_dir"
    find . -maxdepth 1 -type f \
      \( -name '*.deb' -o -name '*.dsc' -o -name '*.changes' -o -name '*.buildinfo' \
         -o -name '*.orig.tar.*' -o -name '*.debian.tar.*' \) \
      -printf '%P\0' | sort -z | xargs -0 -r sha256sum > SHA256SUMS
  )

  python3 - "$artifact_dir" "$version" "$suite" "$arch" <<'PY'
import hashlib
import json
import os
import sys

artifact_dir, version, suite, arch = sys.argv[1:5]
files = []
for name in sorted(os.listdir(artifact_dir)):
    path = os.path.join(artifact_dir, name)
    if not os.path.isfile(path):
        continue
    release_suffixes = (
        ".deb",
        ".dsc",
        ".changes",
        ".buildinfo",
        ".orig.tar.gz",
        ".orig.tar.xz",
        ".orig.tar.bz2",
        ".debian.tar.gz",
        ".debian.tar.xz",
        ".debian.tar.bz2",
    )
    if not name.endswith(release_suffixes):
        continue
    with open(path, "rb") as fh:
        h = hashlib.sha256()
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
        digest = h.hexdigest()
    files.append({"name": name, "sha256": digest, "size": os.path.getsize(path)})

manifest = {
    "package": "rsyslog",
    "version": version,
    "suite": suite,
    "architecture": arch,
    "git_sha": os.environ.get("GITHUB_SHA", ""),
    "github_run_id": os.environ.get("GITHUB_RUN_ID", ""),
    "github_run_number": os.environ.get("GITHUB_RUN_NUMBER", ""),
    "files": files,
}
with open(os.path.join(artifact_dir, "manifest.json"), "w", encoding="utf-8") as fh:
    json.dump(manifest, fh, indent=2, sort_keys=True)
    fh.write("\n")
PY
}

secret_key_fingerprint() {
  gpg --batch --list-secret-keys --with-colons | awk -F: '$1 == "fpr" {print $10; exit}'
}

gpg_sign_args() {
  if [ -n "${DEBIAN_STAGING_GPG_PASSPHRASE:-}" ]; then
    printf '%s\0' --pinentry-mode loopback --passphrase "$DEBIAN_STAGING_GPG_PASSPHRASE"
  fi
}

cmd_generate_repo() {
  local artifact_dir="$1"
  local repo_dir="$2"
  local suite="$3"
  local component="$4"
  local arch="amd64"
  local key_fpr dist_dir packages_file

  [ -d "$artifact_dir" ] || die "missing artifact directory: $artifact_dir"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.deb' | grep -q . || die "no .deb artifacts in $artifact_dir"
  command -v apt-ftparchive >/dev/null 2>&1 || die "apt-ftparchive not found; install apt-utils"

  mkdir -p "$repo_dir/pool/main/r/rsyslog"
  cp -a "$artifact_dir"/*.deb "$repo_dir/pool/main/r/rsyslog/"
  cp -a "$artifact_dir"/manifest.json "$artifact_dir"/SHA256SUMS "$repo_dir/pool/main/r/rsyslog/"

  key_fpr="$(secret_key_fingerprint)"
  [ -n "$key_fpr" ] || die "no GPG secret key available for repository signing"
  gpg --batch --armor --export "$key_fpr" > "$repo_dir/rsyslog-debian-staging.asc"

  dist_dir="$repo_dir/dists/$suite"
  packages_file="$dist_dir/$component/binary-$arch/Packages"
  mkdir -p "$(dirname "$packages_file")"

  (
    cd "$repo_dir"
    apt-ftparchive packages "pool/main/r/rsyslog" > "dists/$suite/$component/binary-$arch/Packages"
    xz -9 -k -f "dists/$suite/$component/binary-$arch/Packages"
    apt-ftparchive \
      -o "APT::FTPArchive::Release::Origin=Adiscon" \
      -o "APT::FTPArchive::Release::Label=rsyslog Debian daily staging" \
      -o "APT::FTPArchive::Release::Suite=$suite" \
      -o "APT::FTPArchive::Release::Codename=$suite" \
      -o "APT::FTPArchive::Release::Architectures=$arch" \
      -o "APT::FTPArchive::Release::Components=$component" \
      -o "APT::FTPArchive::Release::Description=rsyslog Debian daily staging packages" \
      release "dists/$suite" > "dists/$suite/Release"
  )

  mapfile -d '' -t extra_gpg_args < <(gpg_sign_args)
  gpg --batch --yes "${extra_gpg_args[@]}" --local-user "$key_fpr" --clearsign \
    --output "$dist_dir/InRelease" "$dist_dir/Release"
  gpg --batch --yes "${extra_gpg_args[@]}" --local-user "$key_fpr" --detach-sign --armor \
    --output "$dist_dir/Release.gpg" "$dist_dir/Release"
}

cmd_verify_repo() {
  local repo_url="$1"
  local suite="$2"
  local version="$3"
  local expected_key_fpr="$4"
  local tmp_dir
  local packages_url_path="dists/$suite/main/binary-amd64/Packages.xz"
  local packages_release_path="main/binary-amd64/Packages.xz"
  local packages_hash packages_size actual_key_fpr

  command -v curl >/dev/null 2>&1 || die "curl not found"
  command -v gpg >/dev/null 2>&1 || die "gpg not found"
  command -v gpgv >/dev/null 2>&1 || die "gpgv not found"
  command -v xz >/dev/null 2>&1 || die "xz not found"
  [ -n "$expected_key_fpr" ] || die "missing expected signing key fingerprint"

  repo_url="${repo_url%/}"
  tmp_dir="$(mktemp -d)"
  trap 'rm -rf "$tmp_dir"' EXIT

  curl -fsSL "$repo_url/dists/$suite/InRelease" -o "$tmp_dir/InRelease"
  curl -fsSL "$repo_url/rsyslog-debian-staging.asc" -o "$tmp_dir/repo-key.asc"
  gpg --batch --dearmor -o "$tmp_dir/repo-key.gpg" "$tmp_dir/repo-key.asc"
  actual_key_fpr="$(gpg --batch --show-keys --with-colons "$tmp_dir/repo-key.asc" |
    awk -F: '$1 == "fpr" {print $10; exit}')"
  [ "${actual_key_fpr^^}" = "${expected_key_fpr^^}" ] ||
    die "published signing key fingerprint $actual_key_fpr does not match expected $expected_key_fpr"
  gpgv --keyring "$tmp_dir/repo-key.gpg" "$tmp_dir/InRelease"

  curl -fsSL "$repo_url/$packages_url_path" -o "$tmp_dir/Packages.xz"
  packages_hash="$(sha256sum "$tmp_dir/Packages.xz" | awk '{print $1}')"
  packages_size="$(wc -c < "$tmp_dir/Packages.xz" | tr -d ' ')"
  awk -v hash="$packages_hash" -v size="$packages_size" -v path="$packages_release_path" \
    '$1 == hash && $2 == size && $3 == path { found = 1 } END { exit !found }' \
    "$tmp_dir/InRelease" || die "Packages.xz checksum is not present in signed InRelease"
  xz -dc "$tmp_dir/Packages.xz" > "$tmp_dir/Packages"

  grep -Fxq "Package: rsyslog" "$tmp_dir/Packages" || die "rsyslog package not found in Packages"
  grep -Fxq "Version: $version" "$tmp_dir/Packages" || die "version $version not found in Packages"
}

command="${1:-}"
case "$command" in
  version) shift; cmd_version "$@" ;;
  stamp-changelog) shift; cmd_stamp_changelog "$@" ;;
  build-package) shift; cmd_build_package "$@" ;;
  manifest) shift; cmd_manifest "$@" ;;
  generate-repo) shift; cmd_generate_repo "$@" ;;
  verify-repo) shift; cmd_verify_repo "$@" ;;
  *) usage; exit 2 ;;
esac
