# RPM build configuration for rsyslog packaging/rpm/
# Sourced by build-rpms.sh and other scripts.

ARCHOPTIONS="i386 x86_64"
PLATOPTIONS="epel-8 epel-9 rhel-8 rhel-9"

szBaseDir="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
szRpmBaseDir="$szBaseDir/rpmbuild"
szRpmBuildDir="$szRpmBaseDir/SRPMS"
szLocalUser="$(whoami)"

# SPECOPTIONS: discover from rpmbuild/SPECS/ (basenames without .spec)
if [ -d "$szRpmBaseDir/SPECS" ]; then
  SPECOPTIONS="$(find "$szRpmBaseDir/SPECS/" -type f -name "*.spec" -exec basename {} .spec \; 2>/dev/null | sort -u)"
else
  SPECOPTIONS=""
fi

# REPOOPTIONS/szYumRepoDir: optional; main repo has no yumrepo (build-only mode)
if [ -d "$szBaseDir/yumrepo" ]; then
  szYumRepoDir="$szBaseDir/yumrepo"
  REPOOPTIONS="$(find "$szYumRepoDir" -maxdepth 1 -type d -exec basename {} \; 2>/dev/null | grep -v "^yumrepo$" | sort -u)"
else
  szYumRepoDir=""
  REPOOPTIONS=""
fi
