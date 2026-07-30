#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat >&2 <<'EOF'
Usage: fedora-daily-stable.sh <command> [args...]

Commands:
  version
  prepare-sources <baseline-root> <dist-tarball> <output-dir> <policy-file> <feature-contract> <version> <release>
  build-package <prepared-dir> <artifact-dir> <build-log> <expected-evr> <arch>
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
	expected_evr="${version}-${release}.fc44"

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
	local baseline_root="$1"
	local dist_tarball="$2"
	local output_dir="$3"
	local policy_file="$4"
	local feature_contract="$5"
	local version="$6"
	local release="$7"
	local baseline_spec baseline_sources spec_file tmp_dir source_root

	baseline_spec="$baseline_root/SPECS/rsyslog.spec"
	baseline_sources="$baseline_root/SOURCES"
	[ -f "$baseline_spec" ] || die "missing Fedora spec: $baseline_spec"
	[ -d "$baseline_sources" ] || die "missing Fedora sources: $baseline_sources"
	[ -f "$dist_tarball" ] || die "missing dist tarball: $dist_tarball"
	[ -f "$policy_file" ] || die "missing Fedora policy: $policy_file"
	[ -f "$feature_contract" ] || die "missing package feature contract: $feature_contract"

	rm -rf "$output_dir"
	mkdir -p "$output_dir/SOURCES" "$output_dir/SPECS"
	cp -a "$baseline_sources/." "$output_dir/SOURCES/"
	cp "$baseline_spec" "$output_dir/SPECS/rsyslog.spec"
	spec_file="$output_dir/SPECS/rsyslog.spec"

	tmp_dir="$(mktemp -d)"
	trap 'rm -rf "$tmp_dir"' RETURN
	tar -xzf "$dist_tarball" -C "$tmp_dir"
	source_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d -print -quit)"
	[ -n "$source_root" ] || die "dist tarball has no source directory"
	[ "$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | wc -l)" -eq 1 ] ||
		die "dist tarball must contain exactly one source directory"
	mv "$source_root" "$tmp_dir/rsyslog-$version"
	tar -C "$tmp_dir" -czf "$output_dir/SOURCES/rsyslog-$version.tar.gz" \
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

if re.search(r"(?m)^Patch\d*:\s*", spec):
    raise SystemExit("Fedora baseline gained patches; review them before packaging main")

build_requires = policy.get("supplemental_build_requires", [])
packages = [entry.get("package", "").strip() for entry in build_requires]
if any(not package for package in packages):
    raise SystemExit("policy contains an empty supplemental BuildRequires package")
for package in packages:
    if re.search(rf"(?m)^BuildRequires:\s*{re.escape(package)}(?:\s|$)", spec):
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
    raise SystemExit("Fedora spec version/source fields did not match exactly once")

stamp = datetime.now(timezone.utc).strftime("%a %b %d %Y")
entry = (
    f"%changelog\n* {stamp} Adiscon package maintainers "
    f"<release-bot@adiscon.com> - {version}-{release}\n"
    "- Automated rsyslog Fedora 44 daily stable build.\n\n"
)
spec, changelog_count = re.subn(r"(?m)^%changelog\s*$", entry.rstrip(), spec, count=1)
if changelog_count != 1:
    raise SystemExit("Fedora spec has no unique %changelog marker")

spec_path.write_text(spec, encoding="utf-8")
PY

	python3 "$(dirname "$0")/package-feature-overlay.py" rpm \
		"$spec_file" "$feature_contract" rpm

	rm -rf "$tmp_dir"
	trap - RETURN
}

cmd_build_package() {
	local prepared_dir="$1"
	local artifact_dir="$2"
	local build_log="$3"
	local expected_evr="$4"
	local arch="$5"
	local build_root srpm rpm_file actual_evr rc

	command -v dnf >/dev/null || die "dnf is not installed"
	command -v rpmbuild >/dev/null || die "rpmbuild is not installed"
	rm -rf "$artifact_dir"
	mkdir -p "$artifact_dir/srpm" "$artifact_dir/rpms"

	dnf -q -y builddep "$prepared_dir/SPECS/rsyslog.spec"
	set +e
	rpmbuild -bs --define "_topdir $prepared_dir" \
		"$prepared_dir/SPECS/rsyslog.spec" 2>&1 | tee "$build_log"
	rc=${PIPESTATUS[0]}
	set -e
	[ "$rc" -eq 0 ] || return "$rc"

	srpm="$(find "$prepared_dir/SRPMS" -maxdepth 1 -type f -name '*.src.rpm' -print -quit)"
	[ -n "$srpm" ] || die "rpmbuild did not produce a source RPM"
	cp "$srpm" "$artifact_dir/srpm/"

	build_root="$(mktemp -d)"
	trap 'rm -rf "$build_root"' RETURN
	mkdir -p "$build_root/BUILD" "$build_root/BUILDROOT" "$build_root/RPMS" \
		"$build_root/SOURCES" "$build_root/SPECS" "$build_root/SRPMS"
	set +e
	rpmbuild --rebuild --define "_topdir $build_root" "$srpm" \
		2>&1 | tee -a "$build_log"
	rc=${PIPESTATUS[0]}
	set -e
	[ "$rc" -eq 0 ] || return "$rc"

	find "$build_root/RPMS" -type f -name '*.rpm' -exec cp -a {} "$artifact_dir/rpms/" \;
	rpm_file="$(find "$artifact_dir/rpms" -maxdepth 1 -type f \
		-name 'rsyslog-[0-9]*.rpm' ! -name '*-debuginfo-*' ! -name '*-debugsource-*' \
		-print -quit)"
	[ -n "$rpm_file" ] || die "rpmbuild did not produce the base rsyslog RPM"
	[ "$(rpm -qp --qf '%{ARCH}\n' "$rpm_file")" = "$arch" ] ||
		die "base RPM architecture does not match $arch"
	actual_evr="$(rpm -qp --qf '%{VERSION}-%{RELEASE}\n' "$rpm_file")"
	[ "$actual_evr" = "$expected_evr" ] ||
		die "built RPM EVR $actual_evr does not match $expected_evr"
	find "$artifact_dir/rpms" -maxdepth 1 -type f \
		-name 'rsyslog*omazuredce-[0-9]*.rpm' -print -quit | grep -q . ||
		die "omazuredce subpackage RPM was not produced"

	cp "$build_log" "$artifact_dir/build.log"
	cp "$prepared_dir/SPECS/rsyslog.spec" "$artifact_dir/rsyslog.spec"
	rm -rf "$build_root"
	trap - RETURN
}

command="${1:-}"
case "$command" in
	version)
		shift
		cmd_version "$@"
		;;
	prepare-sources)
		shift
		cmd_prepare_sources "$@"
		;;
	build-package)
		shift
		cmd_build_package "$@"
		;;
	sign-rpms | generate-repo | verify-repo | manifest)
		exec "$(dirname "$0")/rpm-daily-stable.sh" "$@"
		;;
	*)
		usage
		exit 2
		;;
esac
