#!/bin/bash
# Copyright 2026 Adiscon GmbH and others
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
# Upload local yumrepo to remote with --delete after confirmation.
# shellcheck disable=SC1090,SC1091
source "$(dirname "$0")/config.sh"

echo "-------------------------------------"
echo "--- Upload RPM Packages (delete)  ---"
echo "-------------------------------------"

if [ -z "${RPM_REPO:-}" ]; then
	echo "Which REPO do you want to upload?--"
	select szSubRepo in $REPOOPTIONS; do
		break
	done
else
	echo "REPO is set to '$RPM_REPO'"
	szSubRepo=$RPM_REPO
fi

if [ -z "${szSubRepo:-}" ]; then
	echo "do_upload_delete: empty repository selection; refusing to continue" >&2
	exit 1
fi

src="$szYumRepoDir/$szSubRepo/"
dst="$REPOUSERNAME@$REPOURL/$szSubRepo/"
echo "Uploading Branch '$src' to $dst"

REPOSSHPORT=${REPOSSHPORT:-22}
RSYNC_E="ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT"
LOCK_FILE="${TMPDIR:-/tmp}/rsyslog-yumrepo-upload-delete.lock"

_run_delete_dry_run() {
	local out=$1
	local status=0
	rsync -au --delete --dry-run -e "$RSYNC_E" \
		--progress "$src" "$dst" >"$out" 2>&1 || status=$?
	return "$status"
}

_deleting_list() {
	# Normalize to a stable sorted list of paths marked for deletion.
	grep 'deleting ' "$1" | sed 's/^.*deleting //' | sort
}

dry_log=$(mktemp)
reval_log=$(mktemp)
trap 'rm -f "$dry_log" "$reval_log"' EXIT

if ! _run_delete_dry_run "$dry_log"; then
	cat "$dry_log" >&2
	echo "do_upload_delete: rsync dry-run FAILED" >&2
	exit 1
fi
FILESTOREMOVE=$(grep -c deleting "$dry_log" || true)
dry_list=$(_deleting_list "$dry_log")

# shellcheck disable=SC2039
read -p "ARE YOU SURE TO REMOVE $FILESTOREMOVE files from $dst " -n 1 -r
echo ""
# shellcheck disable=SC2039
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
	exit 1
fi

# Hold an exclusive local lock across revalidation + destructive sync so
# concurrent local upload/delete helpers cannot widen the race window.
# Remote writers are still unsupported; abort if the deletion set changes.
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
	echo "do_upload_delete: another upload/delete holds $LOCK_FILE; aborting" >&2
	exit 1
fi

if ! _run_delete_dry_run "$reval_log"; then
	cat "$reval_log" >&2
	echo "do_upload_delete: pre-delete revalidation dry-run FAILED" >&2
	exit 1
fi
reval_list=$(_deleting_list "$reval_log")
if [ "$dry_list" != "$reval_list" ]; then
	echo "do_upload_delete: deletion set changed since confirmation; aborting" >&2
	echo "--- confirmed ---" >&2
	printf '%s\n' "$dry_list" >&2
	echo "--- current ---" >&2
	printf '%s\n' "$reval_list" >&2
	exit 1
fi

# Final sync must use the same SSH transport as the dry-runs ($RSYNC_E).
rsync -au --delete -e "$RSYNC_E" --progress "$src" "$dst"
