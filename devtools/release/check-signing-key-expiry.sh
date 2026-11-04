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
# Check that the imported daily-stable archive signing key can still sign.
#
# Usage: check-signing-key-expiry.sh <fingerprint> [warn-days]
#
# Run after importing the secret key into the job's keyring.  Fails with a
# clear message when no usable signing key (primary or subkey) is left, and
# emits a GitHub warning annotation when the last usable one expires within
# warn-days (default 60).  An expired key otherwise only surfaces later as an
# opaque "Unusable secret key" or "gpg exec failed" error during signing.
set -euo pipefail

fingerprint="${1:-}"
warn_days="${2:-60}"
now="${SIGNING_KEY_CHECK_NOW:-$(date -u +%s)}"

if [ -z "$fingerprint" ]; then
	echo "::error title=Archive signing key::no signing key fingerprint configured" >&2
	exit 1
fi
case "$warn_days" in
	''|*[!0-9]*)
		echo "ERROR: warn-days must be a non-negative integer" >&2
		exit 2
		;;
esac

if ! listing="$(gpg --batch --with-colons --fixed-list-mode \
	--list-secret-keys "$fingerprint" 2>/dev/null)"; then
	echo "::error title=Archive signing key::secret key $fingerprint is not in the keyring; check the private key secret and the fingerprint variable" >&2
	exit 1
fi

# Colon fields: 1 type, 2 validity, 7 expiry (epoch, empty = never),
# 12 capabilities.  The primary key line (sec) carries the key's overall
# capabilities in upper case; each key's own ones are lower case.
best=""
best_expiry=""
while IFS=: read -r type validity _ _ _ _ expiry _ _ _ _ caps _; do
	case "$type" in
		sec|ssb) ;;
		*) continue ;;
	esac
	case "$caps" in
		*s*) ;;
		*) continue ;;
	esac
	case "$validity" in
		e|r|i|d|n) continue ;;
	esac
	if [ -n "$expiry" ] && [ "$expiry" -le "$now" ]; then
		continue
	fi
	if [ -z "$expiry" ]; then
		best="never"
		best_expiry=""
		break
	fi
	if [ -z "$best" ] || [ "$expiry" -gt "$best_expiry" ]; then
		best="dated"
		best_expiry="$expiry"
	fi
done <<<"$listing"

if [ -z "$best" ]; then
	echo "::error title=Archive signing key expired::no usable signing key left for $fingerprint (expired, revoked or missing signing capability); create or extend the key and update the private key secret" >&2
	exit 1
fi

if [ "$best" = "never" ]; then
	echo "Archive signing key $fingerprint: usable, no expiry."
	exit 0
fi

expiry_date="$(date -u -d "@$best_expiry" +%Y-%m-%d)"
days_left=$(( (best_expiry - now) / 86400 ))
if [ "$days_left" -lt "$warn_days" ]; then
	echo "::warning title=Archive signing key expires soon::$fingerprint expires on $expiry_date ($days_left days left); extend or replace it before then"
else
	echo "Archive signing key $fingerprint: usable until $expiry_date ($days_left days left)."
fi
