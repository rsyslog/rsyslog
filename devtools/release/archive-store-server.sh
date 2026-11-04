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
# Server side of archive-store.sh's ssh backend.  Install it on the package
# web server and bind the upload key to it in authorized_keys:
#
#   restrict,command="/usr/local/bin/archive-store-server.sh /srv/packages" ssh-ed25519 AAAA...
#
# The key can then only run these requests (from SSH_ORIGINAL_COMMAND):
#   get <path>                  stream the file; exit 3 if it does not exist
#   put-immutable <path> <sha>  store stdin once; an identical existing file
#                               is accepted, a different one fails (exit 4)
#   put-metadata <path> <sha>   atomically replace the file with stdin;
#                               immutable files cannot be replaced
# Paths are relative to the root directory given as the first argument.
# Uploads are checked against the SHA-256 the client sends.
set -euo pipefail

umask 022
max_bytes=$((1024 * 1024 * 1024))

fail() {
	echo "archive-store-server: $2" >&2
	exit "$1"
}

root="${1:-}"
[ -n "$root" ] && [ -d "$root" ] || fail 2 "archive root is missing"
root="${root%/}"

read -r -a request <<<"${SSH_ORIGINAL_COMMAND:-}"
verb="${request[0]:-}"
path="${request[1]:-}"
want="${request[2]:-}"

case "/$path/" in
	//|*/./*|*/../*|*//*) fail 2 "invalid path" ;;
esac
case "$path" in
	/*|*/) fail 2 "invalid path" ;;
esac
[[ "$path" =~ ^[A-Za-z0-9._+~-]+(/[A-Za-z0-9._+~-]+)*$ ]] ||
	fail 2 "invalid path"
target="$root/$path"

# Never follow a symlink below the root, so a request cannot reach outside it.
# Only this script writes below the root, and it never creates symlinks.
check="$root"
IFS=/ read -r -a parts <<<"$path"
for part in "${parts[@]}"; do
	check="$check/$part"
	[ ! -L "$check" ] || fail 2 "symlink in path"
done
[ ! -d "$target" ] || fail 2 "path is a directory"

# Immutable files are stored read-only (0444); metadata stays 0644.
is_immutable() {
	[ -f "$target" ] && [ "$(stat -c %a "$target")" = 444 ]
}

receive() {
	# Store stdin in a temporary file next to the target and check its hash.
	local dir tmp got
	[[ "$want" =~ ^[0-9a-f]{64}$ ]] || fail 2 "invalid sha256"
	dir="$(dirname "$target")"
	mkdir -p "$dir"
	tmp="$(mktemp "$dir/.upload.XXXXXX")"
	head -c "$((max_bytes + 1))" >"$tmp"
	if [ "$(stat -c %s "$tmp")" -gt "$max_bytes" ]; then
		rm -f "$tmp"
		fail 6 "upload too large for $path"
	fi
	got="$(sha256sum "$tmp" | awk '{print $1}')"
	if [ "$got" != "$want" ]; then
		rm -f "$tmp"
		fail 5 "checksum mismatch for $path"
	fi
	chmod "$1" "$tmp"
	printf '%s\n' "$tmp"
}

case "$verb" in
	get)
		[ "${#request[@]}" -eq 2 ] || fail 2 "usage: get <path>"
		[ -f "$target" ] || exit 3
		cat "$target"
		;;
	put-immutable)
		[ "${#request[@]}" -eq 3 ] || fail 2 "usage: put-immutable <path> <sha256>"
		if [ -e "$target" ]; then
			cat >/dev/null
			got="$(sha256sum "$target" | awk '{print $1}')"
			[ "$got" = "$want" ] || fail 4 "immutable archive collision at $path"
			exit 0
		fi
		tmp="$(receive 0444)"
		# A hard link fails if the name appeared meanwhile: never overwrite.
		if ! ln "$tmp" "$target" 2>/dev/null; then
			got="$(sha256sum "$target" | awk '{print $1}')"
			rm -f "$tmp"
			[ "$got" = "$want" ] || fail 4 "immutable archive collision at $path"
			exit 0
		fi
		rm -f "$tmp"
		;;
	put-metadata)
		[ "${#request[@]}" -eq 3 ] || fail 2 "usage: put-metadata <path> <sha256>"
		is_immutable && fail 4 "refusing to replace immutable file $path"
		tmp="$(receive 0644)"
		mv -f "$tmp" "$target"
		;;
	*)
		fail 2 "unknown request"
		;;
esac
