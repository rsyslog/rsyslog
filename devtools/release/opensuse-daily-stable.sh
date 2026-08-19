#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat >&2 <<'EOF'
Usage: opensuse-daily-stable.sh <command> [args...]

Commands:
  version
  prepare-sources <baseline-root> <dist-tarball> <doc-tarball> <output-dir> <policy-file> <feature-contract> <version> <release>
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
	release="0.daily${date}.${run}.${attempt}.git${short_sha}.adiscon1.lp160"
	expected_evr="${version}-${release}"

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
	local doc_tarball="$3"
	local output_dir="$4"
	local policy_file="$5"
	local feature_contract="$6"
	local version="$7"
	local release="$8"
	local baseline_spec baseline_sources spec_file tmp_dir source_root

	baseline_spec="$baseline_root/SPECS/rsyslog.spec"
	baseline_sources="$baseline_root/SOURCES"
	[ -f "$baseline_spec" ] || die "missing openSUSE spec: $baseline_spec"
	[ -d "$baseline_sources" ] || die "missing openSUSE sources: $baseline_sources"
	[ -f "$dist_tarball" ] || die "missing dist tarball: $dist_tarball"
	[ -f "$doc_tarball" ] || die "missing documentation tarball: $doc_tarball"
	[ -f "$policy_file" ] || die "missing openSUSE policy: $policy_file"
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
	cp "$doc_tarball" "$output_dir/SOURCES/rsyslog-doc-$version.tar.gz"

	python3 "$(dirname "$0")/validate-rpm-baseline-patches.py" \
		"$spec_file" "$policy_file" "openSUSE"

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

build_requires = policy.get("supplemental_build_requires", [])
packages = [entry.get("package", "").strip() for entry in build_requires]
if any(not package for package in packages):
    raise SystemExit("policy contains an empty supplemental BuildRequires package")
for package in packages:
    if re.search(rf"(?m)^BuildRequires:\s*{re.escape(package)}(?:\s|$)", spec):
        continue
    spec, count = re.subn(
        r"(?m)^(BuildRequires:\s*bison\s*)$",
        rf"\1\nBuildRequires: {package}",
        spec,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"could not insert supplemental BuildRequires: {package}")

core_files = policy.get("supplemental_core_files", [])
if any(not path.strip() for path in core_files):
    raise SystemExit("policy contains an empty supplemental core file")
for path in core_files:
    if re.search(rf"(?m)^{re.escape(path)}\s*$", spec):
        continue
    spec, count = re.subn(
        r"(?m)^(%\{rsyslog_module_dir_nodeps\}/lmzlibw\.so\s*)$",
        rf"\1\n{path}",
        spec,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"could not add supplemental core file: {path}")

main_files = policy.get("supplemental_main_files", [])
if not isinstance(main_files, list):
    raise SystemExit("supplemental_main_files must be a list")
for entry in main_files:
    if not isinstance(entry, dict):
        raise SystemExit("supplemental main file entry must be an object")
    path = entry.get("path", "").strip()
    anchor = entry.get("anchor", "").strip()
    reason = entry.get("reason", "").strip()
    if not path or not anchor or not reason:
        raise SystemExit("supplemental main file requires path, anchor, and reason")
    if re.search(rf"(?m)^{re.escape(path)}\s*$", spec):
        continue
    spec, count = re.subn(
        rf"(?m)^({re.escape(anchor)}\s*)$",
        rf"\1\n{path}",
        spec,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"could not add supplemental main file: {path}")

spec, macro_count = re.subn(
    r"(?m)^# drop this with next release when doc tarball version lines up\n"
    r"%define rsyslog_major .*\n%define rsyslog_patch .*\n",
    "",
    spec,
)
spec, version_count = re.subn(r"(?m)^Version:\s*.*$", f"Version:        {version}", spec)
spec, release_count = re.subn(
    r"(?m)^Release:\s*.*$", f"Release:        {release}", spec
)
spec, source_count = re.subn(
    r"(?m)^Source0:\s*.*$", "Source0:        %{name}-%{version}.tar.gz", spec
)
spec, doc_source_count = re.subn(
    r"(?m)^Source14:\s*.*$", "Source14:       rsyslog-doc-%{version}.tar.gz", spec
)
if (macro_count, version_count, release_count, source_count, doc_source_count) != (1, 1, 1, 1, 1):
    raise SystemExit("openSUSE spec version/source fields did not match exactly once")

stamp = datetime.now(timezone.utc).strftime("%a %b %d %Y")
entry = (
    f"%changelog\n* {stamp} Adiscon package maintainers "
    f"<release-bot@adiscon.com> - {version}-{release}\n"
    "- Automated rsyslog openSUSE Leap 16.0 daily stable build.\n\n"
)
spec, changelog_count = re.subn(r"(?m)^%changelog\s*$", entry.rstrip(), spec, count=1)
if changelog_count != 1:
    raise SystemExit("openSUSE spec has no unique %changelog marker")

spec_path.write_text(spec, encoding="utf-8")
PY

	python3 "$(dirname "$0")/package-feature-overlay.py" rpm \
		"$spec_file" "$feature_contract" opensuse

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

	command -v rpmbuild >/dev/null || die "rpmbuild is not installed"
	rm -rf "$artifact_dir"
	mkdir -p "$artifact_dir/srpm" "$artifact_dir/rpms"

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
	local package
	for package in module-ossl module-gtls module-omotel module-omazuredce \
		standard full; do
		find "$artifact_dir/rpms" -maxdepth 1 -type f \
			-name "rsyslog-${package}-[0-9]*.rpm" -print -quit | grep -q . ||
			die "rsyslog-$package RPM was not produced"
	done

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
