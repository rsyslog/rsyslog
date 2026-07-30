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
# Sync remote yumrepo into the local mirror, deleting local extras that are
# gone upstream. Requires interactive confirmation after a successful dry-run.
# shellcheck disable=SC1090,SC1091
source "$(dirname "$0")/config.sh"

echo "-------------------------------------"
echo "--- SYNC REMOTE REPO (with delete) ---"
echo "-------------------------------------"

if [ -z "${RPM_REPO:-}" ]; then
	echo "Which REPO do you want to sync from remote?--"
	select szSubRepo in $REPOOPTIONS; do
		break
	done
else
	echo "REPO is set to '$RPM_REPO'"
	szSubRepo=$RPM_REPO
fi

if [ -z "${szSubRepo:-}" ]; then
	echo "sync_remote_delete: empty repository selection; refusing to continue" >&2
	exit 1
fi

REPOSSHPORT=${REPOSSHPORT:-22}
src="$REPOUSERNAME@$REPOURL/$szSubRepo/"
dst="$szYumRepoDir/$szSubRepo/"

echo "SYNC DELETE remote '$src' -> local '$dst'"

dry_log=$(mktemp)
trap 'rm -f "$dry_log"' EXIT
rsync_status=0
rsync -au --delete --dry-run -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" \
	--progress "$src" "$dst" >"$dry_log" 2>&1 || rsync_status=$?
if [ "$rsync_status" -ne 0 ]; then
	cat "$dry_log" >&2
	echo "sync_remote_delete: rsync dry-run FAILED (exit $rsync_status)" >&2
	exit "$rsync_status"
fi
FILESTOREMOVE=$(grep -c deleting "$dry_log" || true)

# shellcheck disable=SC2039
read -p "ARE YOU SURE TO DELETE $FILESTOREMOVE local files under $dst to match remote? " -n 1 -r
echo ""
# shellcheck disable=SC2039
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
	exit 1
fi

rsync -auh --delete -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" --progress \
	"$src" "$dst"
