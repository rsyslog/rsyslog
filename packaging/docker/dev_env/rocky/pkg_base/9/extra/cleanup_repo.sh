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
# Prune old packages from local yumrepo mirrors and rebuild metadata.
# shellcheck disable=SC1090,SC1091
set -euo pipefail
source "$(dirname "$0")/config.sh"

echo "-------------------------------------"
echo "--- CLEANUPREPO                   ---"
echo "-------------------------------------"

if [ -z "${szYumRepoDir:-}" ]; then
	echo "cleanup_repo: szYumRepoDir is not set" >&2
	exit 1
fi

if [ -z "${RPM_PLATFORM:-}" ]; then
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

if [ -z "${RPM_ARCH:-}" ]; then
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

if [ -z "${RPM_REPO:-}" ]; then
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

if [ -z "${RPM_FILESTOKEEP:-}" ]; then
	# shellcheck disable=SC2039
	read -e -p "How many files do you which to keep? " -i "90" RPM_FILESTOKEEP
fi

FILESTOREMOVE=0
for subrepo in $szSubRepo; do
	for distro in $szDist; do
		for arch in $szArch; do
			yumrepotoclean="$szYumRepoDir/$subrepo/$distro/$arch/"
			if [ ! -d "$yumrepotoclean" ]; then
				echo "SKIP missing repo path: $yumrepotoclean"
				continue
			fi
			count=$(repomanage --keep="$RPM_FILESTOKEEP" --old "$yumrepotoclean" | wc -l)
			FILESTOREMOVE=$((FILESTOREMOVE + count))
			echo "Would remove $count packages from $yumrepotoclean"
		done
	done
done

# shellcheck disable=SC2039
read -p "ARE YOU SURE TO REMOVE $FILESTOREMOVE Packages (Keep last $RPM_FILESTOKEEP packages)? " -n 1 -r
echo ""
# shellcheck disable=SC2039
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
	exit 1
fi

for subrepo in $szSubRepo; do
	for distro in $szDist; do
		for arch in $szArch; do
			yumrepotoclean="$szYumRepoDir/$subrepo/$distro/$arch/"
			if [ ! -d "$yumrepotoclean" ]; then
				echo "SKIP missing repo path: $yumrepotoclean"
				continue
			fi
			echo "Cleaning $yumrepotoclean"
			# Fail closed if enumeration or deletion fails.
			mapfile -t old_pkgs < <(repomanage --keep="$RPM_FILESTOKEEP" --old "$yumrepotoclean")
			if [ "${#old_pkgs[@]}" -gt 0 ]; then
				rm -f -- "${old_pkgs[@]}"
			fi
			echo "DELETED ${#old_pkgs[@]} old packages from $yumrepotoclean - rebuild repo now"
			createrepo "$yumrepotoclean"
		done
	done
done

exit 0
