#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: rpm-daily-stable.sh <command> [args...]

Commands:
  version
  prepare-sources <baseline-dir> <dist-tarball> <output-dir> <policy-file> <feature-contract> <version> <release>
  build-package <prepared-dir> <mock-config> <artifact-dir> <build-log> <expected-evr> <arch>
  sign-rpms <artifact-dir> <fingerprint>
  generate-repo <artifact-dir> <repo-dir> <arch> <fingerprint> <passphrase-file>
  verify-repo <repo-url> <arch> <expected-evr> <expected-fingerprint>
  manifest <artifact-dir> <expected-evr> <arch> <channel> <distro> <distro-version>
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
    die "could not determine git commit"
  git rev-parse --short=12 HEAD
}

cmd_version() {
  local version date run attempt short_sha release expected_evr

  version="$(base_version)"
  [ -n "$version" ] || die "could not determine base version"
  date="${RSYSLOG_BUILD_DATE:-$(date -u +%Y%m%d)}"
  printf '%s\n' "$date" | grep -Eq '^[0-9]{8}$' ||
    die "RSYSLOG_BUILD_DATE must use YYYYMMDD"
  run="${GITHUB_RUN_NUMBER:-0}"
  attempt="${GITHUB_RUN_ATTEMPT:-1}"
  short_sha="$(short_commit_sha)"
  release="0.daily${date}.${run}.${attempt}.git${short_sha}.adiscon1"
  expected_evr="${version}-${release}.el10"

  printf '%s\n' "$expected_evr"
  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    {
      printf 'version=%s\n' "$version"
      printf 'release=%s\n' "$release"
      printf 'expected_evr=%s\n' "$expected_evr"
      printf 'archive_date=%s-%s-%s\n' \
        "${date:0:4}" "${date:4:2}" "${date:6:2}"
    } >> "$GITHUB_OUTPUT"
  fi
}

cmd_prepare_sources() {
  local baseline_dir="$1"
  local dist_tarball="$2"
  local output_dir="$3"
  local policy_file="$4"
  local feature_contract="$5"
  local version="$6"
  local release="$7"
  local sources_dir specs_dir spec_file tmp_dir source_root

  [ -f "$baseline_dir/rsyslog.spec" ] ||
    die "missing EL packaging spec: $baseline_dir/rsyslog.spec"
  [ -f "$dist_tarball" ] || die "missing dist tarball: $dist_tarball"
  [ -f "$policy_file" ] || die "missing RPM policy: $policy_file"
  [ -f "$feature_contract" ] || die "missing package feature contract: $feature_contract"

  rm -rf "$output_dir"
  sources_dir="$output_dir/SOURCES"
  specs_dir="$output_dir/SPECS"
  mkdir -p "$sources_dir" "$specs_dir"

  find "$baseline_dir" -maxdepth 1 -type f \
    ! -name rsyslog.spec ! -name sources \
    -exec cp -a {} "$sources_dir/" \;
  cp "$baseline_dir/rsyslog.spec" "$specs_dir/rsyslog.spec"
  spec_file="$specs_dir/rsyslog.spec"

  tmp_dir="$(mktemp -d)"
  trap 'rm -rf "$tmp_dir"' RETURN
  tar -xzf "$dist_tarball" -C "$tmp_dir"
  source_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d -print -quit)"
  [ -n "$source_root" ] || die "dist tarball has no source directory"
  [ "$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | wc -l)" -eq 1 ] ||
    die "dist tarball must contain exactly one source directory"
  mv "$source_root" "$tmp_dir/rsyslog-$version"
  tar -C "$tmp_dir" -czf "$sources_dir/rsyslog-$version.tar.gz" \
    "rsyslog-$version"

  python3 - "$spec_file" "$policy_file" "$version" "$release" <<'PY'
import json
import pathlib
import re
import sys
from datetime import datetime, timezone

spec_path = pathlib.Path(sys.argv[1])
policy_path = pathlib.Path(sys.argv[2])
version = sys.argv[3]
release = sys.argv[4]
spec = spec_path.read_text(encoding="utf-8")
policy = json.loads(policy_path.read_text(encoding="utf-8"))

patches = {}
for match in re.finditer(r"(?m)^Patch(\d+):\s*(\S+)\s*$", spec):
    patches[match.group(2)] = match.group(1)

allowed = policy.get("allowed_patch_skips", [])
allowed_names = [entry.get("patch", "").strip() for entry in allowed]
if any(not name for name in allowed_names):
    raise SystemExit("policy contains an empty allowed patch name")
missing = sorted(set(allowed_names) - set(patches))
if missing:
    raise SystemExit(f"allowed patch is absent from EL10 baseline: {missing}")

for name in allowed_names:
    number = patches[name]
    spec, declaration_count = re.subn(
        rf"(?m)^Patch{re.escape(number)}:\s*{re.escape(name)}\s*\n", "", spec
    )
    spec, application_count = re.subn(
        rf"(?m)^%patch\s+-P{re.escape(number)}(?:\s+[^\n]*)?\s*\n", "", spec
    )
    if declaration_count != 1 or application_count != 1:
        raise SystemExit(
            f"could not remove exactly one declaration/application for {name}"
        )

build_requires = policy.get("supplemental_build_requires", [])
packages = [entry.get("package", "").strip() for entry in build_requires]
if any(not package for package in packages):
    raise SystemExit("policy contains an empty supplemental BuildRequires package")

def has_unconditional_build_requirement(package):
    conditional_depth = 0
    requirement = re.compile(
        rf"^BuildRequires:\s*{re.escape(package)}(?:\s|$)"
    )
    for line in spec.splitlines():
        if re.match(r"^%if(?:arch|narch)?(?:\s|$)", line):
            conditional_depth += 1
            continue
        if re.match(r"^%endif(?:\s|$)", line):
            conditional_depth = max(0, conditional_depth - 1)
            continue
        if conditional_depth == 0 and requirement.match(line):
            return True
    return False

for package in packages:
    if has_unconditional_build_requirement(package):
        continue
    spec, count = re.subn(
        r"(?m)^(BuildRequires:\s*autoconf\s*)$",
        rf"\1\nBuildRequires: {package}",
        spec,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"could not insert supplemental BuildRequires: {package}")

spec, version_count = re.subn(r"(?m)^Version:\s*.*$", f"Version: {version}", spec)
spec, release_count = re.subn(
    r"(?m)^Release:\s*.*$", f"Release: {release}%{{?dist}}", spec
)
spec, source_count = re.subn(
    r"(?m)^Source0:\s*.*$", "Source0: %{name}-%{version}.tar.gz", spec
)
if (version_count, release_count, source_count) != (1, 1, 1):
    raise SystemExit("EL10 spec version/source fields did not match exactly once")

stamp = datetime.now(timezone.utc).strftime("%a %b %d %Y")
entry = (
    f"%changelog\n* {stamp} Adiscon package maintainers "
    f"<release-bot@adiscon.com> - {version}-{release}\n"
    "- Automated rsyslog Enterprise Linux 10 daily stable build.\n\n"
)
spec, changelog_count = re.subn(r"(?m)^%changelog\s*$", entry.rstrip(), spec, count=1)
if changelog_count != 1:
    raise SystemExit("EL10 spec has no unique %changelog marker")

spec_path.write_text(spec, encoding="utf-8")
PY

  python3 "$(dirname "$0")/package-feature-overlay.py" rpm \
    "$spec_file" "$feature_contract" rpm

  while IFS= read -r source_url; do
    case "$source_url" in
      http://*|https://*)
        curl --fail --location --retry 4 --retry-delay 5 \
          --output "$sources_dir/${source_url##*/}" "$source_url"
        ;;
    esac
  done < <(
    rpmspec -P "$spec_file" |
      sed -n 's/^Source[1-9][0-9]*:[[:space:]]*//p'
  )

  rm -rf "$tmp_dir"
  trap - RETURN
}

cmd_build_package() {
  local prepared_dir="$1"
  local mock_config="$2"
  local artifact_dir="$3"
  local build_log="$4"
  local expected_evr="$5"
  local arch="$6"
  local srpm rpm_file actual_evr rc

  rm -rf "$artifact_dir"
  mkdir -p "$artifact_dir/srpm" "$artifact_dir/rpms"

  set +e
  mock -r "$mock_config" --clean --buildsrpm \
    --spec "$prepared_dir/SPECS/rsyslog.spec" \
    --sources "$prepared_dir/SOURCES" \
    --resultdir "$artifact_dir/srpm" 2>&1 | tee "$build_log"
  rc=${PIPESTATUS[0]}
  set -e
  [ "$rc" -eq 0 ] || return "$rc"

  srpm="$(find "$artifact_dir/srpm" -maxdepth 1 -type f -name '*.src.rpm' -print -quit)"
  [ -n "$srpm" ] || die "mock did not produce a source RPM"

  set +e
  mock -r "$mock_config" --clean --rebuild "$srpm" \
    --resultdir "$artifact_dir/rpms" 2>&1 | tee -a "$build_log"
  rc=${PIPESTATUS[0]}
  set -e
  [ "$rc" -eq 0 ] || return "$rc"

  rpm_file="$(find "$artifact_dir/rpms" -maxdepth 1 -type f \
    -name "rsyslog-[0-9]*.${arch}.rpm" ! -name '*-debuginfo-*' -print -quit)"
  [ -n "$rpm_file" ] || die "mock did not produce the base rsyslog RPM"
  actual_evr="$(rpm -qp --qf '%{VERSION}-%{RELEASE}\n' "$rpm_file")"
  [ "$actual_evr" = "$expected_evr" ] ||
    die "built RPM EVR $actual_evr does not match $expected_evr"
  local package
  for package in openssl gnutls omotel omazuredce standard full; do
    find "$artifact_dir/rpms" -maxdepth 1 -type f \
      -name "rsyslog-${package}-[0-9]*.rpm" -print -quit | grep -q . ||
      die "rsyslog-$package RPM was not produced"
  done

  cp "$build_log" "$artifact_dir/build.log"
  cp "$prepared_dir/SPECS/rsyslog.spec" "$artifact_dir/rsyslog.spec"
}

cmd_sign_rpms() {
  local artifact_dir="$1"
  local fingerprint="$2"
  local rpm_file rpm_check

  [ -n "$fingerprint" ] || die "RPM signing fingerprint is empty"
  command -v rpmsign >/dev/null || die "rpmsign is not installed"

  while IFS= read -r -d '' rpm_file; do
    rpmsign --addsign \
      --define "_gpg_name $fingerprint" \
      --define '__gpg /usr/bin/gpg' \
      "$rpm_file"
    [ "$(rpm -qp --qf '%{RSAHEADER:pgpsig}' "$rpm_file")" != '(none)' ] ||
      die "RPM has no RSA header signature: $rpm_file"
    rpm_check="$(rpm --checksig --verbose "$rpm_file" 2>&1 || true)"
    printf '%s\n' "$rpm_check" | grep -q 'Payload SHA256 digest: OK' ||
      die "RPM signature verification failed: $rpm_file"
  done < <(find "$artifact_dir" -type f -name '*.rpm' -print0)
}

create_repo_generation() {
  local package_dir="$1"
  local previous_dir="$2"
  local output_dir="$3"
  local package_kind="$4"
  local new_dir merge_dir

  new_dir="$(mktemp -d)"
  merge_dir="$(mktemp -d)"
  trap 'rm -rf "$new_dir" "$merge_dir"' RETURN
  mkdir -p "$new_dir/Packages"
  case "$package_kind" in
    binary)
      find "$package_dir" -maxdepth 1 -type f -name '*.rpm' \
        ! -name '*.src.rpm' -exec cp -a {} "$new_dir/Packages/" \;
      ;;
    source)
      find "$package_dir" -maxdepth 1 -type f -name '*.src.rpm' \
        -exec cp -a {} "$new_dir/Packages/" \;
      ;;
    *) die "unknown repository package kind: $package_kind" ;;
  esac
  createrepo_c --quiet "$new_dir"

  rm -rf "$output_dir"
  mkdir -p "$output_dir/Packages"
  cp -a "$new_dir/Packages/." "$output_dir/Packages/"
  # An S3 sync of a missing first-run prefix can still leave an empty local
  # repodata directory. Only merge when it contains an actual repository.
  if [ -f "$previous_dir/repodata/repomd.xml" ]; then
    mergerepo_c --all --omit-baseurl \
      --repo "file://$previous_dir" \
      --repo "file://$new_dir" \
      --outputdir "$merge_dir"
    cp -a "$merge_dir/repodata" "$output_dir/repodata"
  else
    cp -a "$new_dir/repodata" "$output_dir/repodata"
  fi

  rm -rf "$new_dir" "$merge_dir"
  trap - RETURN
}

cmd_generate_repo() {
  local artifact_dir="$1"
  local repo_dir="$2"
  local arch="$3"
  local fingerprint="$4"
  local passphrase_file="$5"
  local previous_dir work_dir repo_subdir

  [ -n "$fingerprint" ] || die "repository signing fingerprint is empty"
  [ -n "$repo_dir" ] || die "repository output directory is empty"
  [ -n "$arch" ] || die "repository architecture is empty"
  [ -f "$passphrase_file" ] || die "missing repository signing passphrase file"
  command -v createrepo_c >/dev/null || die "createrepo_c is not installed"
  command -v mergerepo_c >/dev/null || die "mergerepo_c is not installed"

  previous_dir="$(mktemp -d)"
  work_dir="$(mktemp -d)"
  trap 'rm -rf "$previous_dir" "$work_dir"' RETURN
  for repo_subdir in "$arch" SRPMS; do
    mkdir -p "$previous_dir/$repo_subdir"
    if [ -d "$repo_dir/$repo_subdir/repodata" ]; then
      cp -a "$repo_dir/$repo_subdir/repodata" \
        "$previous_dir/$repo_subdir/repodata"
    fi
  done

  create_repo_generation \
    "$artifact_dir/rpms" "$previous_dir/$arch" "$work_dir/$arch" binary
  create_repo_generation \
    "$artifact_dir/srpm" "$previous_dir/SRPMS" "$work_dir/SRPMS" source

  rm -rf "${repo_dir:?}/$arch" "$repo_dir/SRPMS"
  mkdir -p "$repo_dir"
  mv "$work_dir/$arch" "$repo_dir/$arch"
  mv "$work_dir/SRPMS" "$repo_dir/SRPMS"

  for repo_subdir in "$arch" SRPMS; do
    gpg --batch --yes --pinentry-mode loopback \
      --passphrase-file "$passphrase_file" \
      --local-user "$fingerprint" \
      --armor --detach-sign \
      --output "$repo_dir/$repo_subdir/repodata/repomd.xml.asc" \
      "$repo_dir/$repo_subdir/repodata/repomd.xml"
  done
  gpg --batch --armor --export "$fingerprint" \
    > "$repo_dir/rsyslog-archive-keyring.asc"

  rm -rf "$previous_dir" "$work_dir"
  trap - RETURN
}

cmd_verify_repo() {
  local repo_url="$1"
  local arch="$2"
  local expected_evr="$3"
  local expected_fingerprint="$4"
  local verify_dir actual_fingerprint primary_href

  verify_dir="$(mktemp -d)"
  trap 'rm -rf "$verify_dir"' RETURN
  curl --fail --silent --show-error --location \
    "$repo_url/rsyslog-archive-keyring.asc" --output "$verify_dir/key.asc"
  actual_fingerprint="$(
    gpg --batch --show-keys --with-colons "$verify_dir/key.asc" |
      awk -F: '$1 == "fpr" { print $10; exit }'
  )"
  [ "$actual_fingerprint" = "$expected_fingerprint" ] ||
    die "archive key fingerprint $actual_fingerprint does not match expected fingerprint"
  # EL10 GnuPG creates a .kbx file when importing into a custom keyring, while
  # gpgv opens the literal path it is given. Dearmor the public key so every
  # supported verifier gets the same keyring format and filename.
  gpg --batch --yes --dearmor --output "$verify_dir/keyring.gpg" \
    "$verify_dir/key.asc"
  curl --fail --silent --show-error --location \
    "$repo_url/$arch/repodata/repomd.xml" --output "$verify_dir/repomd.xml"
  curl --fail --silent --show-error --location \
    "$repo_url/$arch/repodata/repomd.xml.asc" --output "$verify_dir/repomd.xml.asc"
  gpgv --keyring "$verify_dir/keyring.gpg" \
    "$verify_dir/repomd.xml.asc" "$verify_dir/repomd.xml"
  primary_href="$(
    python3 - "$verify_dir/repomd.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
namespace = {"repo": "http://linux.duke.edu/metadata/repo"}
location = root.find("repo:data[@type='primary']/repo:location", namespace)
if location is None or not location.get("href"):
    raise SystemExit("primary package metadata is missing from repomd.xml")
print(location.get("href"))
PY
  )"
  case "$primary_href" in
    repodata/*/../* | repodata/../* | repodata/*/./*)
      die "unsafe primary metadata path in repomd.xml: $primary_href"
      ;;
    repodata/*) ;;
    *) die "unsafe primary metadata path in repomd.xml: $primary_href" ;;
  esac
  curl --fail --silent --show-error --location \
    "$repo_url/$arch/$primary_href" --output "$verify_dir/primary.xml.gz"
  python3 - "$verify_dir/repomd.xml" "$verify_dir/primary.xml.gz" \
    "$expected_evr" <<'PY'
import gzip
import hashlib
import sys
import xml.etree.ElementTree as ET

repomd_path = sys.argv[1]
metadata_path = sys.argv[2]
expected_evr = sys.argv[3]
repo_namespace = {"repo": "http://linux.duke.edu/metadata/repo"}
common_namespace = {"common": "http://linux.duke.edu/metadata/common"}
repomd = ET.parse(repomd_path).getroot()
primary = repomd.find("repo:data[@type='primary']", repo_namespace)
checksum = primary.find("repo:checksum", repo_namespace) if primary is not None else None
if checksum is None or not checksum.get("type") or not checksum.text:
    raise SystemExit("primary package metadata checksum is missing from repomd.xml")
try:
    with open(metadata_path, "rb") as package_metadata:
        digest = hashlib.new(checksum.get("type"), package_metadata.read()).hexdigest()
except ValueError as error:
    raise SystemExit(f"unsupported primary metadata checksum: {error}") from error
if digest != checksum.text.strip():
    raise SystemExit("primary package metadata checksum does not match repomd.xml")
available = set()
with gzip.open(metadata_path, "rb") as metadata:
    for _, element in ET.iterparse(metadata, events=("end",)):
        if element.tag != "{http://linux.duke.edu/metadata/common}package":
            continue
        name = element.findtext("common:name", namespaces=common_namespace)
        if name == "rsyslog":
            version = element.find("common:version", common_namespace)
            if version is not None:
                available.add(f"{version.get('ver')}-{version.get('rel')}")
        element.clear()
if expected_evr not in available:
    versions = ", ".join(sorted(available)) or "none"
    raise SystemExit(
        f"expected rsyslog EVR {expected_evr} is absent; available EVRs: {versions}"
    )
PY
  printf 'Verified signed repository metadata for %s (%s)\n' \
    "$expected_evr" "$arch"

  rm -rf "$verify_dir"
  trap - RETURN
}

cmd_manifest() {
  local artifact_dir="$1"
  local expected_evr="$2"
  local arch="$3"
  local channel="$4"
  local distro="$5"
  local distro_version="$6"

  python3 - "$artifact_dir" "$expected_evr" "$arch" "$channel" \
    "$distro" "$distro_version" <<'PY'
import hashlib
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest = {
    "version": sys.argv[2],
    "architecture": sys.argv[3],
    "channel": sys.argv[4],
    "distribution": sys.argv[5],
    "distribution_version": sys.argv[6],
    "files": [],
}
for path in sorted(root.rglob("*")):
    if not path.is_file() or path.name in {"manifest.json", "SHA256SUMS"}:
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest["files"].append(
        {"path": str(path.relative_to(root)), "sha256": digest, "size": path.stat().st_size}
    )
(root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
with (root / "SHA256SUMS").open("w", encoding="utf-8") as output:
    for entry in manifest["files"]:
        output.write(f"{entry['sha256']}  {entry['path']}\n")
PY
}

command="${1:-}"
case "$command" in
  version) shift; cmd_version "$@" ;;
  prepare-sources) shift; cmd_prepare_sources "$@" ;;
  build-package) shift; cmd_build_package "$@" ;;
  sign-rpms) shift; cmd_sign_rpms "$@" ;;
  generate-repo) shift; cmd_generate_repo "$@" ;;
  verify-repo) shift; cmd_verify_repo "$@" ;;
  manifest) shift; cmd_manifest "$@" ;;
  *) usage; exit 2 ;;
esac
