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
echo "--- resignrepo                    ---"
echo "-------------------------------------"

if [ -z "$RPM_PLATFORM" ]; then
	echo "Which Linux Plattform?:"
	select szDist in $PLATOPTIONS "All"; do
		case $szDist in
		"All")
			szDist=$PLATOPTIONS
			;;
		esac
		echo "Making RPM for EPEL '$szDist'"
		break
	done
else
	echo "PLATFORM is set to '$RPM_PLATFORM'"
	szDist=$RPM_PLATFORM
fi

if [ -z "$RPM_ARCH" ]; then
	echo "Which Architecture?:"
	select szArch in $ARCHOPTIONS "All"; do
		case $szArch in
		"All")
			szArch=$ARCHOPTIONS
			;;
		esac
		echo "Making RPM for Plattforms '$szArch'"
		break
	done
else
	echo "ARCH is set to '$RPM_ARCH'"
	szArch=$RPM_ARCH
fi

if [ -z "$RPM_REPO" ]; then
	echo "Which YUM repository?:"
	select szSubRepo in $REPOOPTIONS "All"; do
		case $szSubRepo in
		"All")
			szSubRepo=$REPOOPTIONS
			;;
		esac
		break
	done
else
	echo "REPO is set to '$RPM_REPO'"
	szSubRepo=$RPM_REPO
fi

for subrepo in $szSubRepo; do
	for distro in $szDist; do
		for arch in $szArch; do
			echo "Updating REPO $distro-$arch-$subrepo"

			repo=$szYumRepoDir/$subrepo/$distro/$arch
			if [ ! -d "$repo/RPMS" ]; then
				echo "SKIP missing repo path: $repo"
				continue
			fi
			if ! sudo rpm --addsign "$repo"/RPMS/*.rpm; then
				echo "rpmsign FAILED for $repo"
				exit 1
			fi
			if ! sudo createrepo -s sha256 -o "$repo" -d -p "$repo"; then
				echo "createrepo FAILED for $repo"
				exit 1
			fi
			sudo rm -f "$repo"/repodata/repomd.xml.asc
			# Prefer GPG_PASSPHRASE (CI/daily); fallback to mounted passfile.txt.
			# GnuPG 2.1+ needs batch + loopback for non-interactive passphrase use.
			if [ -n "${GPG_PASSPHRASE:-}" ]; then
				if ! echo "$GPG_PASSPHRASE" | sudo gpg --batch --pinentry-mode loopback \
					--passphrase-fd 0 --detach-sign --armor \
					"$repo"/repodata/repomd.xml; then
					echo "gpg sign FAILED for $repo/repodata/repomd.xml"
					exit 1
				fi
			elif [ -f passfile.txt ]; then
				if ! sudo gpg --batch --pinentry-mode loopback \
					--passphrase-file passfile.txt --detach-sign --armor \
					"$repo"/repodata/repomd.xml; then
					echo "gpg sign FAILED for $repo/repodata/repomd.xml"
					exit 1
				fi
			else
				echo "gpg sign FAILED: set GPG_PASSPHRASE or provide passfile.txt" >&2
				exit 1
			fi
		done
	done
done

exit 0
