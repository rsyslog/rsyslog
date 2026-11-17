#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Build and check a signed archive publication bundle (format v1).
#
# A publish job does not upload to the package server.  It collects the
# files of one publication in a bundle and hands it over as a GitHub
# Actions artifact; a poller on the server verifies and applies it.
#
# Usage:
#   archive-publication.sh init <dir> <prefix> <run-id> <run-attempt>
#   archive-publication.sh add <immutable|metadata> <dir> <local-file> <path>
#   archive-publication.sh sign <dir> <ssh-private-key>
#   archive-publication.sh verify <dir> <allowed-signers-file>
#
# <path> for "add" is relative to <prefix>.  Add all immutable files first,
# then the metadata, signed APT/RPM/APK metadata last (InRelease last of
# all).  Nothing can be added after an InRelease entry.
#
# Bundle layout:
#   manifest      rsyslog-archive-publication v1
#                 prefix <prefix>
#                 run <run-id> <run-attempt>
#                 created <YYYY-MM-DDTHH:MM:SSZ>
#                 immutable|metadata <sha256> <size> <prefix>/<path>  (1/file)
#   manifest.sig  ssh-keygen -Y sign -n rsyslog-archive-publication
#   files/<prefix>/<path>
#
# Paths follow the rules of archive-store-server.sh.  "verify" checks the
# signature (principal and namespace rsyslog-archive-publication), the
# manifest syntax and order, and the SHA-256 and size of every file; it is
# the reference for the server-side poller.  Needs bash, coreutils, awk and
# ssh-keygen (OpenSSH 8.1 or later).
set -euo pipefail

format_line="rsyslog-archive-publication v1"
namespace="rsyslog-archive-publication"
principal="rsyslog-archive-publication"

die() {
	echo "archive-publication: $*" >&2
	exit 1
}

# Same rules as archive-store-server.sh.
valid_path() {
	case "/$1/" in
		//|*/./*|*/../*|*//*) return 1 ;;
	esac
	case "$1" in
		/*|*/) return 1 ;;
	esac
	[[ "$1" =~ ^[A-Za-z0-9._+~-]+(/[A-Za-z0-9._+~-]+)*$ ]]
}

manifest_prefix() {
	awk 'NR == 2 && $1 == "prefix" && NF == 2 { print $2; found = 1 }
		END { exit !found }' "$1/manifest" ||
		die "$1/manifest has no prefix line"
}

cmd_init() {
	[ "$#" -eq 4 ] || die "usage: init <dir> <prefix> <run-id> <run-attempt>"
	local dir="$1" prefix="$2" run_id="$3" run_attempt="$4"

	valid_path "$prefix" || die "invalid prefix: $prefix"
	[[ "$run_id" =~ ^[0-9]+$ ]] || die "invalid run id: $run_id"
	[[ "$run_attempt" =~ ^[0-9]+$ ]] || die "invalid run attempt: $run_attempt"
	if [ -e "$dir" ]; then
		if [ ! -d "$dir" ] || [ -n "$(ls -A "$dir")" ]; then
			die "bundle directory exists and is not empty: $dir"
		fi
	fi
	mkdir -p "$dir/files"
	{
		printf '%s\n' "$format_line"
		printf 'prefix %s\n' "$prefix"
		printf 'run %s %s\n' "$run_id" "$run_attempt"
		printf 'created %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
	} >"$dir/manifest"
}

cmd_add() {
	[ "$#" -eq 4 ] ||
		die "usage: add <immutable|metadata> <dir> <local-file> <path>"
	local kind="$1" dir="$2" local_file="$3" rel="$4"
	local prefix path state sha size

	case "$kind" in
		immutable|metadata) ;;
		*) die "unknown entry kind: $kind" ;;
	esac
	[ -f "$dir/manifest" ] || die "not an initialized bundle: $dir"
	[ ! -e "$dir/manifest.sig" ] || die "bundle is already signed: $dir"
	if [ ! -f "$local_file" ] || [ -L "$local_file" ]; then
		die "not a regular file: $local_file"
	fi
	prefix="$(manifest_prefix "$dir")"
	path="$prefix/$rel"
	if ! valid_path "$rel" || ! valid_path "$path"; then
		die "invalid path: $rel"
	fi

	# Prints "duplicate", "inrelease" (an InRelease entry exists, so the
	# bundle is complete) or "metadata" (a metadata entry exists).
	state="$(awk -v p="$path" '
		NR > 4 && $4 == p { print "duplicate"; exit }
		NR > 4 && $4 ~ /(^|\/)InRelease$/ { last = "inrelease" }
		NR > 4 && $1 == "metadata" && last == "" { last = "metadata" }
		END { if (last != "") print last }' "$dir/manifest" | head -n 1)"
	case "$state" in
		duplicate) die "duplicate path: $path" ;;
		inrelease) die "nothing may follow InRelease: $path" ;;
		metadata)
			[ "$kind" = metadata ] ||
				die "immutable entry after metadata: $path"
			;;
	esac

	mkdir -p "$(dirname "$dir/files/$path")"
	cp "$local_file" "$dir/files/$path"
	sha="$(sha256sum "$dir/files/$path" | awk '{print $1}')"
	size="$(wc -c <"$dir/files/$path" | tr -d ' ')"
	printf '%s %s %s %s\n' "$kind" "$sha" "$size" "$path" >>"$dir/manifest"
}

cmd_sign() {
	[ "$#" -eq 2 ] || die "usage: sign <dir> <ssh-private-key>"
	local dir="$1" key="$2"

	[ -f "$dir/manifest" ] || die "not an initialized bundle: $dir"
	[ "$(wc -l <"$dir/manifest")" -gt 4 ] || die "bundle has no files: $dir"
	[ -f "$key" ] || die "missing signing key: $key"
	rm -f "$dir/manifest.sig"
	ssh-keygen -q -Y sign -n "$namespace" -f "$key" "$dir/manifest" ||
		die "signing $dir/manifest failed"
	[ -s "$dir/manifest.sig" ] || die "ssh-keygen wrote no signature"
}

cmd_verify() {
	[ "$#" -eq 2 ] || die "usage: verify <dir> <allowed-signers-file>"
	local dir="$1" signers="$2"
	local manifest="$dir/manifest" listed actual

	if [ ! -f "$manifest" ] || [ -L "$manifest" ]; then
		die "missing manifest"
	fi
	[ -f "$dir/manifest.sig" ] || die "missing manifest.sig"
	[ -f "$signers" ] || die "missing allowed signers file: $signers"
	ssh-keygen -Y verify -f "$signers" -I "$principal" -n "$namespace" \
		-s "$dir/manifest.sig" <"$manifest" >/dev/null ||
		die "manifest signature does not verify"

	[ -z "$(tail -c 1 "$manifest")" ] || die "manifest does not end with LF"
	! grep -q $'\r' "$manifest" || die "manifest contains CR characters"

	local line_no=0 line prefix="" kind sha size path seen_metadata=0
	local seen_inrelease=0 got_sha got_size
	declare -A seen=()
	while IFS= read -r line; do
		line_no=$((line_no + 1))
		case "$line_no" in
			1)
				[ "$line" = "$format_line" ] ||
					die "manifest line 1 is not '$format_line'"
				continue
				;;
			2)
				[[ "$line" =~ ^prefix\ ([^ ]+)$ ]] ||
					die "manifest line 2 is not a prefix line"
				prefix="${BASH_REMATCH[1]}"
				valid_path "$prefix" || die "invalid prefix: $prefix"
				continue
				;;
			3)
				[[ "$line" =~ ^run\ [0-9]+\ [0-9]+$ ]] ||
					die "manifest line 3 is not a run line"
				continue
				;;
			4)
				[[ "$line" =~ ^created\ [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$ ]] ||
					die "manifest line 4 is not a created line"
				continue
				;;
		esac
		[[ "$line" =~ ^(immutable|metadata)\ ([0-9a-f]{64})\ (0|[1-9][0-9]*)\ ([^ ]+)$ ]] ||
			die "malformed manifest line $line_no"
		kind="${BASH_REMATCH[1]}"
		sha="${BASH_REMATCH[2]}"
		size="${BASH_REMATCH[3]}"
		path="${BASH_REMATCH[4]}"
		valid_path "$path" || die "invalid path: $path"
		case "$path" in
			"$prefix"/*) ;;
			*) die "path outside prefix $prefix: $path" ;;
		esac
		[ -z "${seen[$path]:-}" ] || die "duplicate path: $path"
		seen[$path]=1
		[ "$seen_inrelease" -eq 0 ] || die "entry after InRelease: $path"
		if [ "$kind" = metadata ]; then
			seen_metadata=1
		elif [ "$seen_metadata" -eq 1 ]; then
			die "immutable entry after metadata: $path"
		fi
		case "$path" in
			InRelease|*/InRelease) seen_inrelease=1 ;;
		esac
		if [ ! -f "$dir/files/$path" ] || [ -L "$dir/files/$path" ]; then
			die "missing file: files/$path"
		fi
		got_size="$(wc -c <"$dir/files/$path" | tr -d ' ')"
		[ "$got_size" = "$size" ] || die "size mismatch: $path"
		got_sha="$(sha256sum "$dir/files/$path" | awk '{print $1}')"
		[ "$got_sha" = "$sha" ] || die "sha256 mismatch: $path"
	done <"$manifest"
	[ "$line_no" -gt 4 ] || die "manifest lists no files"

	listed="${#seen[@]}"
	actual="$(find "$dir/files" -type f | wc -l | tr -d ' ')"
	[ "$listed" = "$actual" ] || die "files/ holds files not in the manifest"
	[ -z "$(find "$dir/files" ! -type f ! -type d -print -quit)" ] ||
		die "files/ holds something other than files and directories"
}

command="${1:-}"
[ "$#" -gt 0 ] && shift
case "$command" in
	init) cmd_init "$@" ;;
	add) cmd_add "$@" ;;
	sign) cmd_sign "$@" ;;
	verify) cmd_verify "$@" ;;
	*) die "usage: archive-publication.sh init|add|sign|verify ..." ;;
esac
