#!/usr/bin/env bash
set -euo pipefail

usage() {
	cat >&2 <<'EOF'
Usage: alpine-daily-stable.sh <command> [args...]

Commands:
  version
  prepare-sources <baseline-dir> <dist-tarball> <output-dir> <policy-file> <feature-contract> <pkgver>
  verify-artifacts <artifact-dir> <expected-version> <arch>
  verify-published <repo-url> <origin-url> <arch> <expected-version> <public-key-sha256>
  generate-index <artifact-dir> <previous-index> <repo-dir> <arch> <private-key> <public-key>
  manifest <artifact-dir> <expected-version> <arch> <channel> <distro> <distro-version>
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

cmd_version() {
	local base date run attempt serial pkgver expected_version

	base="$(base_version)"
	[ -n "$base" ] || die "could not determine base version"
	date="${RSYSLOG_BUILD_DATE:-$(date -u +%Y%m%d)}"
	printf '%s\n' "$date" | grep -Eq '^[0-9]{8}$' ||
		die "RSYSLOG_BUILD_DATE must use YYYYMMDD"
	run="${GITHUB_RUN_NUMBER:-0}"
	attempt="${GITHUB_RUN_ATTEMPT:-1}"
	printf '%s\n' "$run" | grep -Eq '^[0-9]+$' ||
		die "GITHUB_RUN_NUMBER must be numeric"
	printf '%s\n' "$attempt" | grep -Eq '^[0-9]+$' ||
		die "GITHUB_RUN_ATTEMPT must be numeric"
	[ "$attempt" -le 99 ] || die "GITHUB_RUN_ATTEMPT exceeds two digits"
	serial="$(printf '%s%010d%02d' "$date" "$run" "$attempt")"
	pkgver="${base}_p${serial}"
	expected_version="${pkgver}-r0"

	printf '%s\n' "$expected_version"
	if [ -n "${GITHUB_OUTPUT:-}" ]; then
		{
			printf 'pkgver=%s\n' "$pkgver"
			printf 'expected_version=%s\n' "$expected_version"
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
	local pkgver="$6"
	local tmp_dir source_root

	[ -f "$baseline_dir/APKBUILD" ] ||
		die "missing Alpine packaging baseline: $baseline_dir/APKBUILD"
	[ -f "$dist_tarball" ] || die "missing dist tarball: $dist_tarball"
	[ -f "$policy_file" ] || die "missing Alpine packaging policy: $policy_file"
	[ -f "$feature_contract" ] || die "missing package feature contract: $feature_contract"

	rm -rf "$output_dir"
	mkdir -p "$output_dir"
	find "$baseline_dir" -maxdepth 1 -type f -exec cp -a {} "$output_dir/" \;

	tmp_dir="$(mktemp -d)"
	trap 'rm -rf "$tmp_dir"' RETURN
	tar -xzf "$dist_tarball" -C "$tmp_dir"
	source_root="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d -print -quit)"
	[ -n "$source_root" ] || die "dist tarball has no source directory"
	[ "$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | wc -l)" -eq 1 ] ||
		die "dist tarball must contain exactly one source directory"
	mv "$source_root" "$tmp_dir/rsyslog-$pkgver"
	tar -C "$tmp_dir" -czf "$output_dir/rsyslog-$pkgver.tar.gz" \
		"rsyslog-$pkgver"

	python3 - "$output_dir/APKBUILD" "$policy_file" "$pkgver" <<'PY'
import json
import pathlib
import re
import sys

path = pathlib.Path(sys.argv[1])
policy = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
pkgver = sys.argv[3]
text = path.read_text(encoding="utf-8")

for entry in policy.get("supplemental_makedepends", []):
    package = entry.get("package", "").strip()
    if not package:
        raise SystemExit("policy contains an empty supplemental makedepends package")
    if re.search(rf"(?m)^\s*{re.escape(package)}\s*$", text):
        continue
    text, count = re.subn(
        r"(?m)^(\s*autoconf\s*)$",
        rf"\1\n\t{package}",
        text,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"could not insert supplemental makedepends: {package}")

text, version_count = re.subn(r"(?m)^pkgver=.*$", f"pkgver={pkgver}", text)
text, release_count = re.subn(r"(?m)^pkgrel=.*$", "pkgrel=0", text)
text, source_count = re.subn(
    r'(?m)^source="\$pkgname-\$pkgver\.tar\.gz::[^\n]+$',
    'source="$pkgname-$pkgver.tar.gz',
    text,
)
if (version_count, release_count, source_count) != (1, 1, 1):
    raise SystemExit("Alpine APKBUILD version/source fields did not match exactly once")

# abuild checksum recreates this block from the prepared local source files.
text, checksum_count = re.subn(r'(?ms)^sha512sums=".*?"\s*$', "", text)
if checksum_count != 1:
    raise SystemExit("Alpine APKBUILD has no unique sha512sums block")
path.write_text(text.rstrip() + "\n", encoding="utf-8")
PY

	python3 "$(dirname "$0")/package-feature-overlay.py" alpine \
		"$output_dir/APKBUILD" "$feature_contract"

	rm -rf "$tmp_dir"
	trap - RETURN
}

cmd_verify_artifacts() {
	local artifact_dir="$1"
	local expected_version="$2"
	local arch="$3"
	local base_apk

	base_apk="$(find "$artifact_dir" -maxdepth 1 -type f \
		-name "rsyslog-${expected_version}.apk" -print -quit)"
	[ -n "$base_apk" ] ||
		die "base rsyslog APK for $expected_version is absent"
	local package
	for package in openssl gnutls omotel omazuredce standard full; do
		[ -f "$artifact_dir/rsyslog-$package-$expected_version.apk" ] ||
			die "rsyslog-$package APK for $expected_version is absent"
	done
	if ! apk verify "$base_apk"; then
		die "base rsyslog APK failed signature or integrity verification"
	fi
	find "$artifact_dir" -maxdepth 1 -type f -name '*.apk' -print -quit |
		grep -q . || die "no APK artifacts were produced"
	[ "$(apk --print-arch)" = "$arch" ] ||
		die "builder architecture $(apk --print-arch) does not match $arch"
}

cmd_verify_published() {
	local repo_url="$1"
	local origin_url="$2"
	local arch="$3"
	local expected_version="$4"
	local expected_public_key_sha256="$5"
	local attempt installed_version module_version verification_rc=1

	[ -n "$repo_url" ] || die "missing public Alpine repository URL"
	[ -n "$origin_url" ] || die "missing Alpine repository origin URL"
	curl -fsSL "$repo_url/rsyslog-alpine-archive.rsa.pub" \
		-o /tmp/rsyslog-alpine-cdn-key.pub
	[ "$(sha256sum /tmp/rsyslog-alpine-cdn-key.pub | awk '{print $1}')" = \
		"$expected_public_key_sha256" ] || die "CDN public key digest mismatch"
	curl -fsSL "$repo_url/$arch/APKINDEX.tar.gz" \
		-o /tmp/rsyslog-alpine-cdn-index.tar.gz
	tar -tzf /tmp/rsyslog-alpine-cdn-index.tar.gz | grep -Fxq APKINDEX ||
		die "CDN repository index is invalid"

	for attempt in $(seq 1 20); do
		rm -f /tmp/rsyslog-alpine-archive.rsa.pub
		if curl -fsSL "$origin_url/rsyslog-alpine-archive.rsa.pub" \
			-o /tmp/rsyslog-alpine-archive.rsa.pub &&
			[ "$(sha256sum /tmp/rsyslog-alpine-archive.rsa.pub | awk '{print $1}')" = \
				"$expected_public_key_sha256" ]; then
			cp /tmp/rsyslog-alpine-archive.rsa.pub /etc/apk/keys/
			if ! grep -Fxq "$origin_url" /etc/apk/repositories; then
				echo "$origin_url" >> /etc/apk/repositories
			fi
			if apk update --no-cache &&
				apk add --no-cache \
					"rsyslog=$expected_version" \
					"rsyslog-standard=$expected_version"; then
				verification_rc=0
				break
			fi
		fi
		echo "Origin repository not ready yet, retrying ($attempt/20)..."
		[ "$attempt" -eq 20 ] || sleep 30
	done
	[ "$verification_rc" -eq 0 ] || die "could not install the expected published packages"
	installed_version="$(
		apk query --from installed --fields version --format json rsyslog |
			sed -n 's/.*"version": "\([^"]*\)".*/\1/p'
	)"
	[ "$installed_version" = "$expected_version" ] || die "installed rsyslog version mismatch"
	local package module_file
	for package in openssl gnutls omotel standard; do
		module_version="$(
			apk query --from installed --fields version --format json "rsyslog-$package" |
				sed -n 's/.*"version": "\([^"]*\)".*/\1/p'
		)"
		[ "$module_version" = "$expected_version" ] ||
			die "installed rsyslog-$package version mismatch"
	done
	apk add --no-cache "rsyslog-full=$expected_version" ||
		die "could not install the expected full profile package"
	full_dependencies="$(apk info -R --from installed rsyslog-full)"
	for package in rsyslog rsyslog-openssl rsyslog-gnutls rsyslog-omotel rsyslog-omazuredce; do
		printf '%s\n' "$full_dependencies" | grep -Fq "$package=$expected_version" ||
			die "rsyslog-full is missing its $package dependency"
	done
	for package in omazuredce full; do
		module_version="$(
			apk query --from installed --fields version --format json "rsyslog-$package" |
				sed -n 's/.*"version": "\([^"]*\)".*/\1/p'
		)"
		[ "$module_version" = "$expected_version" ] ||
			die "installed rsyslog-$package version mismatch"
	done
	for package in openssl gnutls omotel omazuredce; do
		case "$package" in
			openssl) module_file=lmnsd_ossl.so ;;
			gnutls) module_file=lmnsd_gtls.so ;;
			*) module_file=$package.so ;;
		esac
		module_file="${module_file//./\\.}"
		apk info -L "rsyslog-$package" | grep -Eq "/rsyslog/$module_file$" ||
			die "$package module file is absent"
	done
	rsyslogd -v
	"$(dirname "$0")/package-feature-smoke.sh"
}

cmd_generate_index() {
	local artifact_dir="$1"
	local previous_index="$2"
	local repo_dir="$3"
	local arch="$4"
	local private_key="$5"
	local public_key="$6"
	local index_path keys_dir

	[ -f "$private_key" ] || die "missing Alpine repository private key"
	[ -f "$public_key" ] || die "missing Alpine repository public key"
	mkdir -p "$repo_dir/$arch"
	cp -a "$artifact_dir"/*.apk "$repo_dir/$arch/"
	index_path="$repo_dir/$arch/APKINDEX.tar.gz"
	keys_dir="$(mktemp -d)"
	trap 'rm -rf "$keys_dir"' RETURN
	cp "$public_key" "$keys_dir/$(basename "$public_key")"
	if [ -s "$previous_index" ]; then
		apk --keys-dir "$keys_dir" index --merge --index "$previous_index" \
			--no-warnings \
			--description "rsyslog Alpine daily stable" \
			--output "$index_path" "$repo_dir/$arch"/*.apk
	else
		apk --keys-dir "$keys_dir" index \
			--no-warnings \
			--description "rsyslog Alpine daily stable" \
			--output "$index_path" "$repo_dir/$arch"/*.apk
	fi
	abuild-sign --type RSA256 --private "$private_key" \
		--public "$(basename "$public_key")" "$index_path"
	cp "$public_key" "$repo_dir/$(basename "$public_key")"
	rm -rf "$keys_dir"
	trap - RETURN
}

cmd_manifest() {
	local artifact_dir="$1"
	local expected_version="$2"
	local arch="$3"
	local channel="$4"
	local distro="$5"
	local distro_version="$6"

	python3 - "$artifact_dir" "$expected_version" "$arch" "$channel" \
		"$distro" "$distro_version" <<'PY'
import hashlib
import json
import os
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest = {
    "version": sys.argv[2],
    "architecture": sys.argv[3],
    "channel": sys.argv[4],
    "distribution": sys.argv[5],
    "distribution_version": sys.argv[6],
    "source_commit": os.environ.get("SOURCE_GIT_SHA", ""),
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
	verify-artifacts) shift; cmd_verify_artifacts "$@" ;;
	verify-published) shift; cmd_verify_published "$@" ;;
	generate-index) shift; cmd_generate_index "$@" ;;
	manifest) shift; cmd_manifest "$@" ;;
	*) usage; exit 2 ;;
esac
