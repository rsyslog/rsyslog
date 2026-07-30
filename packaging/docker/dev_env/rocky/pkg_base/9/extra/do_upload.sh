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
# Definitions common to these scripts
# shellcheck disable=SC1090,SC1091
source "$(dirname "$0")/config.sh"

echo "-------------------------------------"
echo "--- Upload RPM Packages           ---"
echo "-------------------------------------"

if [ -z "$RPM_REPO" ]; then
	echo "Which REPO do you want to upload?--"
	select szSubRepo in $REPOOPTIONS; do
		break
	done
else
	echo "REPO is set to '$RPM_REPO'"
	szSubRepo=$RPM_REPO
fi

echo "Uploading Branch '$szYumRepoDir/$szSubRepo/' to $REPOUSERNAME@$REPOURL/$szSubRepo/"

# Set default SSH port to 22 if REPOSSHPORT is not set
REPOSSHPORT=${REPOSSHPORT:-22}

rsync -au -e "ssh -i /private-files/.ssh/id_rsa -p $REPOSSHPORT" --progress \
	"$szYumRepoDir/$szSubRepo/"* "$REPOUSERNAME@$REPOURL/$szSubRepo/"
