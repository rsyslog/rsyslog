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
# Storage backend for the daily-stable package archives.
#
# Usage:
#   archive-store.sh fetch <remote-path> <local-file>
#   archive-store.sh put-immutable <local-file> <remote-path>
#   archive-store.sh put-metadata <local-file> <remote-path>
#
# <remote-path> is relative to the target's archive prefix, for example
# dists/trixie/InRelease.  "fetch" exits 0 and leaves <local-file> absent
# when the object does not exist.  "put-immutable" never replaces an object:
# an identical existing object is accepted, a different one is an error.
# "put-metadata" replaces the object; publish it last.
#
# ARCHIVE_BACKEND selects the backend:
#   s3   DigitalOcean Spaces or any S3 store.  Needs SPACE_ENDPOINT,
#        SPACE_BUCKET, SPACE_PREFIX and AWS credentials in the environment.
#   ssh  A web server reached over SSH.  The server runs
#        archive-store-server.sh as the key's forced command.  Needs
#        ARCHIVE_SSH_TARGET (user@host) and ARCHIVE_PREFIX; optional
#        ARCHIVE_SSH_KEY, ARCHIVE_SSH_KNOWN_HOSTS and, for tests only,
#        ARCHIVE_SSH_COMMAND to replace the ssh client.
set -euo pipefail

die() {
	echo "archive-store: $*" >&2
	exit 1
}

check_path() {
	case "/$1/" in
		//|*/./*|*/../*|*//*) die "invalid remote path: $1" ;;
	esac
	case "$1" in
		/*|*/) die "invalid remote path: $1" ;;
	esac
}

# ---- S3 backend -----------------------------------------------------------

s3_key() {
	printf '%s/%s\n' "${SPACE_PREFIX:?SPACE_PREFIX is not set}" "$1"
}

s3_exists() {
	local key="$1" found
	found="$(
		aws s3api list-objects-v2 \
			--endpoint-url "${SPACE_ENDPOINT:?}" \
			--bucket "${SPACE_BUCKET:?}" \
			--prefix "$key" \
			--max-keys 1 \
			--query 'Contents[0].Key' \
			--output text
	)" || die "S3 lookup of $key failed"
	[ "$found" = "$key" ]
}

s3_fetch() {
	local key
	key="$(s3_key "$1")"
	s3_exists "$key" || return 0
	aws s3 cp --only-show-errors --endpoint-url "$SPACE_ENDPOINT" \
		"s3://$SPACE_BUCKET/$key" "$2"
}

s3_put_immutable() {
	local key local_hash remote_hash
	key="$(s3_key "$2")"
	local_hash="$(sha256sum "$1" | awk '{print $1}')"
	if s3_exists "$key"; then
		remote_hash="$(
			aws s3 cp --quiet --endpoint-url "$SPACE_ENDPOINT" \
				"s3://$SPACE_BUCKET/$key" - | sha256sum | awk '{print $1}'
		)"
		[ "$remote_hash" = "$local_hash" ] ||
			die "immutable archive collision at $key"
		return 0
	fi
	aws s3 cp --only-show-errors --endpoint-url "$SPACE_ENDPOINT" \
		--acl public-read \
		--cache-control 'public,max-age=31536000,immutable' \
		--metadata "sha256=$local_hash,max-age=31536000" \
		"$1" "s3://$SPACE_BUCKET/$key"
}

s3_put_metadata() {
	aws s3 cp --only-show-errors --endpoint-url "$SPACE_ENDPOINT" \
		--acl public-read \
		--cache-control 'public,max-age=60,must-revalidate' \
		--metadata 'max-age=60' \
		"$1" "s3://$SPACE_BUCKET/$(s3_key "$2")"
}

# ---- SSH backend ----------------------------------------------------------

ssh_run() {
	local -a cmd
	if [ -n "${ARCHIVE_SSH_COMMAND:-}" ]; then
		# shellcheck disable=SC2206 # test hook, intentionally word-split
		cmd=($ARCHIVE_SSH_COMMAND)
	else
		cmd=(ssh -o BatchMode=yes -o ConnectTimeout=20)
		if [ -n "${ARCHIVE_SSH_KEY:-}" ]; then
			cmd+=(-o IdentitiesOnly=yes -i "$ARCHIVE_SSH_KEY")
		fi
		if [ -n "${ARCHIVE_SSH_KNOWN_HOSTS:-}" ]; then
			cmd+=(-o StrictHostKeyChecking=yes
				-o "UserKnownHostsFile=$ARCHIVE_SSH_KNOWN_HOSTS")
		fi
		cmd+=("${ARCHIVE_SSH_TARGET:?ARCHIVE_SSH_TARGET is not set}")
	fi
	"${cmd[@]}" "$*"
}

ssh_path() {
	printf '%s/%s\n' "${ARCHIVE_PREFIX:?ARCHIVE_PREFIX is not set}" "$1"
}

ssh_fetch() {
	local tmp rc=0
	tmp="$2.part"
	ssh_run get "$(ssh_path "$1")" >"$tmp" || rc=$?
	case "$rc" in
		0) mv "$tmp" "$2" ;;
		3) rm -f "$tmp" ;;  # not found
		*) rm -f "$tmp"; die "fetch of $1 failed (exit $rc)" ;;
	esac
}

ssh_put_immutable() {
	local hash
	hash="$(sha256sum "$1" | awk '{print $1}')"
	ssh_run put-immutable "$(ssh_path "$2")" "$hash" <"$1"
}

ssh_put_metadata() {
	local hash
	hash="$(sha256sum "$1" | awk '{print $1}')"
	ssh_run put-metadata "$(ssh_path "$2")" "$hash" <"$1"
}

# ---- dispatch -------------------------------------------------------------

[ "$#" -eq 3 ] || die "usage: archive-store.sh fetch|put-immutable|put-metadata <a> <b>"
backend="${ARCHIVE_BACKEND:-s3}"
case "$backend" in
	s3|ssh) ;;
	*) die "unknown ARCHIVE_BACKEND: $backend" ;;
esac

case "$1" in
	fetch)
		check_path "$2"
		"${backend}_fetch" "$2" "$3"
		;;
	put-immutable)
		[ -f "$2" ] || die "missing local file: $2"
		check_path "$3"
		"${backend}_put_immutable" "$2" "$3"
		;;
	put-metadata)
		[ -f "$2" ] || die "missing local file: $2"
		check_path "$3"
		"${backend}_put_metadata" "$2" "$3"
		;;
	*)
		die "unknown command: $1"
		;;
esac
