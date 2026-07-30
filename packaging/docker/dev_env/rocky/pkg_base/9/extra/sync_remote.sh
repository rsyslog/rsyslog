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
# Sync remote yumrepo into the local mirror (update only, no delete).
# shellcheck disable=SC1090,SC1091
set -euo pipefail

if [ -z "${PKGBASEDIR:-}" ]; then
	echo "sync_remote: PKGBASEDIR is not set; refusing to sync" >&2
	exit 1
fi
if [ ! -d "$PKGBASEDIR" ]; then
	echo "sync_remote: PKGBASEDIR '$PKGBASEDIR' is not a directory" >&2
	exit 1
fi
if [ ! -f "$PKGBASEDIR/config.sh" ]; then
	echo "sync_remote: missing $PKGBASEDIR/config.sh" >&2
	exit 1
fi

szBaseDir=$PKGBASEDIR
source "$szBaseDir/config.sh"
cd "$szBaseDir/" || exit 1
echo "-------------------------------------"
echo "--- Basedir: $szBaseDir"
echo "--- Sync remote repository        ---"
echo "-------------------------------------"

if [ -z "${RPM_REPO:-}" ]; then
	echo "sync_remote: RPM_REPO not set"
	addrepo=
	addplatform=
	addarch=
else
	echo "sync_remote: RPM_REPO set to              $RPM_REPO"
	addrepo=$RPM_REPO/
	if [ -z "${RPM_PLATFORM:-}" ]; then
		echo "sync_remote: RPM_PLATFORM not set"
		addplatform=
		addarch=
	else
		echo "sync_remote: RPM_PLATFORM set to  $RPM_PLATFORM"
		addplatform=$RPM_PLATFORM/
		if [ -z "${RPM_ARCH:-}" ]; then
			echo "sync_remote: RPM_ARCH not set"
			addarch=
		else
			echo "sync_remote: RPM_ARCH set to              $RPM_ARCH"
			addarch=$RPM_ARCH/
		fi
	fi
fi

REPOSSHPORT=${REPOSSHPORT:-22}

echo "rsync $REPOUSERNAME@$REPOURL/$addrepo$addplatform$addarch to $szBaseDir/yumrepo/$addrepo$addplatform$addarch"
rsync -avh --update -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" --progress \
	"$REPOUSERNAME@$REPOURL/$addrepo$addplatform$addarch" \
	"$szBaseDir/yumrepo/$addrepo$addplatform$addarch"
