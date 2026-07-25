#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: debian-daily-stable.sh <command> [args...]

Commands:
  version
  stamp-changelog <source-dir> <version>
  build-package <workspace> <source-dir> <dist-tarball> <artifact-dir> <version> <build-log>
  manifest <artifact-dir> <version> <suite> <arch> <channel> <distro> <distro-version>
  generate-repo <artifact-dir> <repo-dir> <suite> <component> <arch>
  verify-repo <repo-url> <suite> <component> <arch> <version> <expected-key-fingerprint> [require-source-package]
  self-test
EOF
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

base_version() {
  local configure_file script_dir

  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  configure_file="${RSYSLOG_SOURCE_DIR:-$script_dir/../..}/configure.ac"
  sed -n 's/AC_INIT(\[rsyslog\],\[\([^]]*\)\].*/\1/p' "$configure_file" |
    sed 's/\.daily$//'
}

short_commit_sha() {
  local candidate="${SOURCE_GIT_SHA:-${GITHUB_SHA:-}}"

  if [ -n "$candidate" ] &&
     printf '%s\n' "$candidate" | grep -Eq '^[0-9a-fA-F]{12,64}$'; then
    printf '%.12s\n' "$candidate" | tr '[:upper:]' '[:lower:]'
    return
  fi

  git rev-parse --verify HEAD >/dev/null 2>&1 ||
    die "could not determine git commit from SOURCE_GIT_SHA, GITHUB_SHA, or local HEAD"
  git rev-parse --short=12 HEAD
}

cmd_version() {
  local base date short_sha run attempt version

  base="$(base_version)"
  [ -n "$base" ] || die "could not determine base version from configure.ac"

  date="${RSYSLOG_BUILD_DATE:-$(date -u +%Y%m%d)}"
  printf '%s\n' "$date" | grep -Eq '^[0-9]{8}$' ||
    die "RSYSLOG_BUILD_DATE must use YYYYMMDD"
  short_sha="$(short_commit_sha)"
  run="${GITHUB_RUN_NUMBER:-0}"
  attempt="${GITHUB_RUN_ATTEMPT:-1}"
  version="${base}~daily${date}.${run}.${attempt}+git${short_sha}-1adiscon1"

  printf '%s\n' "$version"

  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    {
      printf 'version=%s\n' "$version"
      printf 'upstream_version=%s\n' "${version%%-*}"
      printf 'base_version=%s\n' "$base"
      printf 'archive_date=%s-%s-%s\n' \
        "${date:0:4}" "${date:4:2}" "${date:6:2}"
    } >> "$GITHUB_OUTPUT"
  fi
}

cmd_stamp_changelog() {
  local source_dir="$1"
  local version="$2"

  [ -d "$source_dir/debian" ] ||
    die "missing debian directory: $source_dir/debian"

  cd "$source_dir"
  export DEBEMAIL="${DEBEMAIL:-release-bot@adiscon.com}"
  export DEBFULLNAME="${DEBFULLNAME:-Adiscon package maintainers}"
  dch --force-distribution --distribution trixie --newversion "$version" \
    "Automated rsyslog Debian daily stable build."
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

  [ -f "$workspace/$dist_tarball" ] ||
    die "missing dist tarball: $workspace/$dist_tarball"
  [ -d "$source_dir/debian" ] ||
    die "missing Debian source tree: $source_dir"
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

  find "$artifact_dir" -maxdepth 1 -type f -name '*.deb' | grep -q . ||
    die "no .deb artifacts collected"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.dsc' | grep -q . ||
    die "no .dsc artifact collected"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.changes' | grep -q . ||
    die "no .changes artifact collected"
}

cmd_manifest() {
  local artifact_dir="$1"
  local version="$2"
  local suite="$3"
  local arch="$4"
  local channel="$5"
  local distro="$6"
  local distro_version="$7"

  [ -d "$artifact_dir" ] ||
    die "missing artifact directory: $artifact_dir"

  (
    cd "$artifact_dir"
    find . -maxdepth 1 -type f \
      \( -name '*.deb' -o -name '*.dsc' -o -name '*.changes' -o -name '*.buildinfo' \
         -o -name '*.orig.tar.*' -o -name '*.debian.tar.*' \) \
      -printf '%P\0' | sort -z | xargs -0 -r sha256sum > SHA256SUMS
  )

  python3 - "$artifact_dir" "$version" "$suite" "$arch" \
    "$channel" "$distro" "$distro_version" <<'PY'
import hashlib
import json
import os
import sys

artifact_dir, version, suite, arch, channel, distro, distro_version = sys.argv[1:8]
files = []
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
for name in sorted(os.listdir(artifact_dir)):
    path = os.path.join(artifact_dir, name)
    if not os.path.isfile(path) or not name.endswith(release_suffixes):
        continue
    with open(path, "rb") as fh:
        digest = hashlib.file_digest(fh, "sha256").hexdigest()
    files.append({"name": name, "sha256": digest, "size": os.path.getsize(path)})

manifest = {
    "schema": 1,
    "package": "rsyslog",
    "version": version,
    "channel": channel,
    "distribution": distro,
    "distribution_version": distro_version,
    "suite": suite,
    "architecture": arch,
    "retention": "minimum-five-years-from-publication",
    "git_sha": os.environ.get("SOURCE_GIT_SHA", os.environ.get("GITHUB_SHA", "")),
    "github_run_id": os.environ.get("GITHUB_RUN_ID", ""),
    "github_run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT", ""),
    "github_run_number": os.environ.get("GITHUB_RUN_NUMBER", ""),
    "files": files,
}
with open(os.path.join(artifact_dir, "manifest.json"), "w", encoding="utf-8") as fh:
    json.dump(manifest, fh, indent=2, sort_keys=True)
    fh.write("\n")
PY
}

secret_key_fingerprint() {
  gpg --batch --list-secret-keys --with-colons |
    awk -F: '$1 == "fpr" {print $10; exit}'
}

gpg_sign_args() {
  if [ -n "${DEBIAN_DAILY_STABLE_GPG_PASSPHRASE:-}" ]; then
    printf '%s\0' --pinentry-mode loopback \
      --passphrase "$DEBIAN_DAILY_STABLE_GPG_PASSPHRASE"
  fi
}

restore_plain_index() {
  local index_path="$1"

  if [ ! -f "$index_path" ] && [ -f "$index_path.xz" ]; then
    xz -dc "$index_path.xz" > "$index_path"
  elif [ ! -f "$index_path" ]; then
    : > "$index_path"
  fi
}

merge_deb822_indexes() {
  local existing_file="$1"
  local new_file="$2"
  local output_file="$3"
  local kind="$4"

  python3 - "$existing_file" "$new_file" "$output_file" "$kind" <<'PY'
import sys

existing_path, new_path, output_path, kind = sys.argv[1:5]


def paragraphs(path):
    with open(path, "r", encoding="utf-8") as stream:
        text = stream.read().strip()
    return [] if not text else text.split("\n\n")


def fields(paragraph):
    parsed = {}
    current = None
    for line in paragraph.splitlines():
        if line.startswith((" ", "\t")) and current:
            parsed[current] += "\n" + line
            continue
        if ":" not in line:
            continue
        current, value = line.split(":", 1)
        parsed[current] = value.strip()
    return parsed


def key(paragraph):
    data = fields(paragraph)
    if kind == "binary":
        names = ("Package", "Version", "Architecture")
    else:
        names = ("Package", "Version")
    values = tuple(data.get(name, "") for name in names)
    if not all(values):
        raise SystemExit(f"incomplete {kind} index paragraph: {values}")
    return values


merged = {}
order = []
for path in (existing_path, new_path):
    for paragraph in paragraphs(path):
        paragraph_key = key(paragraph)
        if paragraph_key not in merged:
            order.append(paragraph_key)
        merged[paragraph_key] = paragraph

with open(output_path, "w", encoding="utf-8") as stream:
    for paragraph_key in order:
        stream.write(merged[paragraph_key].rstrip() + "\n\n")
PY
}

compress_index() {
  local index_path="$1"

  gzip -9 -n -c "$index_path" > "$index_path.gz"
  xz -9 -c "$index_path" > "$index_path.xz"
}

add_by_hash() {
  local index_path="$1"
  local hash_dir="$2"
  local candidate digest

  mkdir -p "$hash_dir"
  for candidate in "$index_path" "$index_path.gz" "$index_path.xz"; do
    digest="$(sha256sum "$candidate" | awk '{print $1}')"
    cp "$candidate" "$hash_dir/$digest"
  done
}

copy_pool_artifacts() {
  local artifact_dir="$1"
  local pool_dir="$2"

  mkdir -p "$pool_dir"
  find "$artifact_dir" -maxdepth 1 -type f \
    \( -name '*.deb' -o -name '*.dsc' -o -name '*.orig.tar.*' \
       -o -name '*.debian.tar.*' \) \
    -exec cp -a {} "$pool_dir/" \;
}

cmd_generate_repo() {
  local artifact_dir="$1"
  local repo_dir="$2"
  local suite="$3"
  local component="$4"
  local arch="$5"
  local key_fpr dist_dir pool_dir binary_dir source_dir
  local packages_file sources_file

  [ -d "$artifact_dir" ] ||
    die "missing artifact directory: $artifact_dir"
  find "$artifact_dir" -maxdepth 1 -type f -name '*.deb' | grep -q . ||
    die "no .deb artifacts in $artifact_dir"
  command -v apt-ftparchive >/dev/null 2>&1 ||
    die "apt-ftparchive not found; install apt-utils"

  pool_dir="$repo_dir/pool/$component/r/rsyslog"
  dist_dir="$repo_dir/dists/$suite"
  binary_dir="$dist_dir/$component/binary-$arch"
  source_dir="$dist_dir/$component/source"
  packages_file="$binary_dir/Packages"
  sources_file="$source_dir/Sources"
  mkdir -p "$binary_dir" "$source_dir"

  restore_plain_index "$packages_file"
  restore_plain_index "$sources_file"
  copy_pool_artifacts "$artifact_dir" "$pool_dir"

  (
    cd "$repo_dir"
    apt-ftparchive packages "pool/$component/r/rsyslog" > "$packages_file.new"
    if find "pool/$component/r/rsyslog" -maxdepth 1 -type f -name '*.dsc' |
       grep -q .; then
      apt-ftparchive sources "pool/$component/r/rsyslog" > "$sources_file.new"
    else
      : > "$sources_file.new"
    fi
  )

  merge_deb822_indexes "$packages_file" "$packages_file.new" \
    "$packages_file.merged" binary
  merge_deb822_indexes "$sources_file" "$sources_file.new" \
    "$sources_file.merged" source
  mv "$packages_file.merged" "$packages_file"
  mv "$sources_file.merged" "$sources_file"
  rm -f "$packages_file.new" "$sources_file.new"
  compress_index "$packages_file"
  compress_index "$sources_file"

  (
    cd "$repo_dir"
    apt-ftparchive \
      -o "APT::FTPArchive::Release::Origin=Adiscon" \
      -o "APT::FTPArchive::Release::Label=rsyslog daily stable" \
      -o "APT::FTPArchive::Release::Suite=$suite" \
      -o "APT::FTPArchive::Release::Codename=$suite" \
      -o "APT::FTPArchive::Release::Architectures=$arch" \
      -o "APT::FTPArchive::Release::Components=$component" \
      -o "APT::FTPArchive::Release::Acquire-By-Hash=yes" \
      -o "APT::FTPArchive::Release::Description=rsyslog Debian daily stable packages" \
      release "dists/$suite" > "dists/$suite/Release"
  )

  add_by_hash "$packages_file" "$binary_dir/by-hash/SHA256"
  add_by_hash "$sources_file" "$source_dir/by-hash/SHA256"

  key_fpr="$(secret_key_fingerprint)"
  [ -n "$key_fpr" ] ||
    die "no GPG secret key available for repository signing"
  gpg --batch --armor --export "$key_fpr" > "$repo_dir/rsyslog-archive-keyring.asc"

  mapfile -d '' -t extra_gpg_args < <(gpg_sign_args)
  gpg --batch --yes "${extra_gpg_args[@]}" --local-user "$key_fpr" \
    --clearsign --output "$dist_dir/InRelease" "$dist_dir/Release"
  gpg --batch --yes "${extra_gpg_args[@]}" --local-user "$key_fpr" \
    --detach-sign --armor --output "$dist_dir/Release.gpg" "$dist_dir/Release"
}

cmd_verify_repo() (
  local repo_url="$1"
  local suite="$2"
  local component="$3"
  local arch="$4"
  local version="$5"
  local expected_key_fpr="$6"
  local require_source_package="${7:-false}"
  local tmp_dir packages_url_path packages_release_path
  local sources_url_path sources_release_path
  local packages_hash packages_size sources_hash sources_size
  local actual_key_fpr cleanup_command

  command -v curl >/dev/null 2>&1 || die "curl not found"
  command -v gpg >/dev/null 2>&1 || die "gpg not found"
  command -v gpgv >/dev/null 2>&1 || die "gpgv not found"
  command -v xz >/dev/null 2>&1 || die "xz not found"
  [ -n "$expected_key_fpr" ] ||
    die "missing expected signing key fingerprint"
  case "$require_source_package" in
    true|false) ;;
    *) die "require-source-package must be true or false" ;;
  esac

  repo_url="${repo_url%/}"
  packages_url_path="dists/$suite/$component/binary-$arch/Packages.xz"
  packages_release_path="$component/binary-$arch/Packages.xz"
  sources_url_path="dists/$suite/$component/source/Sources.xz"
  sources_release_path="$component/source/Sources.xz"
  tmp_dir="$(mktemp -d)"
  printf -v cleanup_command 'rm -rf -- %q' "$tmp_dir"
  # Expand now because function-local variables are unavailable to an EXIT trap.
  # shellcheck disable=SC2064
  trap "$cleanup_command" EXIT

  curl -fsSL "$repo_url/dists/$suite/InRelease" -o "$tmp_dir/InRelease"
  curl -fsSL "$repo_url/rsyslog-archive-keyring.asc" -o "$tmp_dir/repo-key.asc"
  gpg --batch --dearmor -o "$tmp_dir/repo-key.gpg" "$tmp_dir/repo-key.asc"
  actual_key_fpr="$(
    gpg --batch --show-keys --with-colons "$tmp_dir/repo-key.asc" |
      awk -F: '$1 == "fpr" {print $10; exit}'
  )"
  [ "${actual_key_fpr^^}" = "${expected_key_fpr^^}" ] ||
    die "published signing key fingerprint $actual_key_fpr does not match expected $expected_key_fpr"
  gpgv --keyring "$tmp_dir/repo-key.gpg" "$tmp_dir/InRelease"

  curl -fsSL "$repo_url/$packages_url_path" -o "$tmp_dir/Packages.xz"
  packages_hash="$(sha256sum "$tmp_dir/Packages.xz" | awk '{print $1}')"
  packages_size="$(wc -c < "$tmp_dir/Packages.xz" | tr -d ' ')"
  awk -v hash="$packages_hash" -v size="$packages_size" \
    -v path="$packages_release_path" \
    '$1 == hash && $2 == size && $3 == path { found = 1 } END { exit !found }' \
    "$tmp_dir/InRelease" ||
    die "Packages.xz checksum is not present in signed InRelease"
  xz -dc "$tmp_dir/Packages.xz" > "$tmp_dir/Packages"

  grep -Fxq "Package: rsyslog" "$tmp_dir/Packages" ||
    die "rsyslog package not found in Packages"
  grep -Fxq "Version: $version" "$tmp_dir/Packages" ||
    die "version $version not found in Packages"

  curl -fsSL "$repo_url/$sources_url_path" -o "$tmp_dir/Sources.xz"
  sources_hash="$(sha256sum "$tmp_dir/Sources.xz" | awk '{print $1}')"
  sources_size="$(wc -c < "$tmp_dir/Sources.xz" | tr -d ' ')"
  awk -v hash="$sources_hash" -v size="$sources_size" \
    -v path="$sources_release_path" \
    '$1 == hash && $2 == size && $3 == path { found = 1 } END { exit !found }' \
    "$tmp_dir/InRelease" ||
    die "Sources.xz checksum is not present in signed InRelease"
  xz -dc "$tmp_dir/Sources.xz" > "$tmp_dir/Sources"
  if [ "$require_source_package" = true ]; then
    grep -Fxq "Package: rsyslog" "$tmp_dir/Sources" ||
      die "rsyslog source package not found in Sources"
    grep -Fxq "Version: $version" "$tmp_dir/Sources" ||
      die "source version $version not found in Sources"
  fi

  rm -rf "$tmp_dir"
  trap - EXIT
)

build_self_test_deb() {
  local root="$1"
  local version="$2"
  local package_dir="$root/package-$version"

  install -m 755 -d "$package_dir/DEBIAN"
  {
    echo "Package: rsyslog"
    echo "Version: $version"
    echo "Architecture: amd64"
    echo "Maintainer: rsyslog test <test@example.invalid>"
    echo "Description: synthetic daily archive test package"
  } > "$package_dir/DEBIAN/control"
  dpkg-deb --build "$package_dir" "$root/rsyslog_${version}_amd64.deb" >/dev/null
}

cmd_self_test() (
  local tmp_dir repo_dir first_artifacts second_artifacts fingerprint
  local cleanup_command

  for tool in apt-ftparchive curl dpkg-deb gpg gpgv xz; do
    command -v "$tool" >/dev/null 2>&1 ||
      die "$tool is required for self-test"
  done

  tmp_dir="$(mktemp -d)"
  printf -v cleanup_command 'rm -rf -- %q' "$tmp_dir"
  # Expand now because function-local variables are unavailable to an EXIT trap.
  # shellcheck disable=SC2064
  trap "$cleanup_command" EXIT
  repo_dir="$tmp_dir/repo"
  first_artifacts="$tmp_dir/artifacts-1"
  second_artifacts="$tmp_dir/artifacts-2"
  mkdir -p "$repo_dir" "$first_artifacts" "$second_artifacts"
  export GNUPGHOME="$tmp_dir/gnupg"
  install -m 700 -d "$GNUPGHOME"
  gpg --batch --pinentry-mode loopback --passphrase '' --quick-generate-key \
    "rsyslog archive self-test <test@example.invalid>" rsa2048 sign 0 >/dev/null 2>&1
  fingerprint="$(secret_key_fingerprint)"

  build_self_test_deb "$first_artifacts" "1.0~daily1"
  cmd_generate_repo "$first_artifacts" "$repo_dir" trixie main amd64
  cmd_verify_repo "file://$repo_dir" trixie main amd64 \
    "1.0~daily1" "$fingerprint"

  build_self_test_deb "$second_artifacts" "1.0~daily2"
  cmd_generate_repo "$second_artifacts" "$repo_dir" trixie main amd64
  cmd_verify_repo "file://$repo_dir" trixie main amd64 \
    "1.0~daily1" "$fingerprint"
  cmd_verify_repo "file://$repo_dir" trixie main amd64 \
    "1.0~daily2" "$fingerprint"

  rm -rf "$tmp_dir"
  trap - EXIT
  echo "Debian daily stable archive self-test passed."
)

command="${1:-}"
case "$command" in
  version) shift; cmd_version "$@" ;;
  stamp-changelog) shift; cmd_stamp_changelog "$@" ;;
  build-package) shift; cmd_build_package "$@" ;;
  manifest) shift; cmd_manifest "$@" ;;
  generate-repo) shift; cmd_generate_repo "$@" ;;
  verify-repo) shift; cmd_verify_repo "$@" ;;
  self-test) shift; cmd_self_test "$@" ;;
  *) usage; exit 2 ;;
esac
