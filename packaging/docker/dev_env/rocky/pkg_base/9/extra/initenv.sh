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
# Initialize the daily RPM package-build container environment.
# shellcheck disable=SC1090,SC1091
set -euo pipefail
source "$(dirname "$0")/config.sh"
if [ -z "${PKGGITBRANCH:-}" ]; then
	echo "initenv: use master (default)"
	gitbranch=master
else
	echo "initenv: set to $PKGGITBRANCH"
	gitbranch=$PKGGITBRANCH
fi

echo "---------------------------------------------"
echo "--- Copy private files to their locations ---"
cp -rf /home/pkg/.rpmmacros /root/
ln -sf /private-files/passfile.txt "$szBaseDir/passfile.txt"
cp -rf /private-files/.gnupg /home/pkg/.gnupg/
# Optional extra key material; empty dir is fine.
cp -rf /private-files/.gnupg/* /home/pkg/.gnupg/ 2>/dev/null || true
cp -rf /private-files/.gnupg /root/.gnupg/
cp -rf /private-files/.gnupg/* /root/.gnupg/ 2>/dev/null || true
cp -rf /private-files/.ssh /root/
cp -rf /private-files/.ssh /home/pkg/
# Copy entitlements and subscription manager configurations (RHEL mock builds).
# Mount layout is expected to be directory trees whose *contents* map onto
# /etc/pki/entitlement, /etc/rhsm/, and /etc/rhsm/ca/.
if [ -d /private-files/rhel/etc-pki-entitlement ]; then
	mkdir -p /etc/pki/entitlement
	cp -rf /private-files/rhel/etc-pki-entitlement/. /etc/pki/entitlement/
fi
if [ -d /private-files/rhel/rhsm-conf ]; then
	mkdir -p /etc/rhsm
	cp -rf /private-files/rhel/rhsm-conf/. /etc/rhsm/
fi
if [ -d /private-files/rhel/rhsm-ca ]; then
	mkdir -p /etc/rhsm/ca
	cp -rf /private-files/rhel/rhsm-ca/. /etc/rhsm/ca/
fi
echo "---------------------------------------------"

echo "--------------------------------"
echo "--- Setting file permissions ---"
# GnuPG needs execute on directories; keep key material private.
for gpgdir in /home/pkg/.gnupg /root/.gnupg; do
	if [ -d "$gpgdir" ]; then
		find "$gpgdir" -type d -exec chmod 700 {} +
		find "$gpgdir" -type f -exec chmod 600 {} +
	fi
done
# Credential trees under /home/pkg must be owned by pkg (copies above run as root).
chown -R pkg:pkg /home/pkg/.gnupg /home/pkg/.ssh /home/pkg/.rpmmacros 2>/dev/null || true
# Own only direct children of /private-files. Do NOT glob .*
# (that expands to .. and would chown /).
find /private-files -mindepth 1 -maxdepth 1 -exec chown -R pkg:pkg {} + 2>/dev/null || true
chmod 0600 /private-files/.ssh/id_rsa 2>/dev/null || true
chmod 0600 /private-files/.ssh/id_dsa 2>/dev/null || true
chmod 0600 /home/pkg/.ssh/id_rsa 2>/dev/null || true
chmod 0600 /home/pkg/.ssh/id_dsa 2>/dev/null || true
chmod 0600 /root/.ssh/id_rsa 2>/dev/null || true
chmod 0600 /root/.ssh/id_dsa 2>/dev/null || true
echo "--------------------------------"

echo "--------------------------------"
echo "--- Sync RPM REPO"
./sync_remote.sh
echo "--------------------------------"
echo "--- Sync GIT and change branch"
git fetch --all

if test "$gitbranch" = "master"; then
	if test -z "${gitdebugbranch:-}"; then
		echo "initenv: using master branch"
		git checkout -f "$gitbranch"
		git pull
	else
		echo "initenv: using custom branch $gitdebugbranch"
		git pull
		git checkout "$gitdebugbranch"
	fi
else
	echo "initenv: switch to PR branch"
	git fetch -t https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git "$gitbranch"
	git reset --hard FETCH_HEAD --
	git checkout -B "$gitbranch"
	git rev-parse HEAD
fi

# Set PKGNOREPLACEMOCKFILES to keep system mock-core-configs files
if [ -z "${PKGNOREPLACEMOCKFILES:-}" ]; then
	echo "initenv: replace MOCK files with our own (default)"
	cp -rf etc-mock/* /etc/mock/
	chown -R pkg ./
else
	echo "initenv: use system MOCK files"
fi
