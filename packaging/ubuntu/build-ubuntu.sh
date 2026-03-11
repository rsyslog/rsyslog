#!/bin/bash
# Build Ubuntu .deb packages from current source.
# Usage: ./build-ubuntu.sh [--source-only] [--suite SUITE]
#   --source-only: build source package only (no binary, no docker)
#   --suite SUITE: focal, jammy, or noble (default: prompt or UBUNTU_SUITE env)
# Env: UBUNTU_SUITE, UBUNTU_VERSION (optional override, default: from configure.ac + timestamp)
#
# Run from repo root or packaging/ubuntu/. Requires: dpkg-dev, wget (for jammy/noble qpid-proton), (docker for binary build).

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
source "$SCRIPT_DIR/config.sh"
cd "$REPO_ROOT"

SOURCE_ONLY=false
SUITE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --source-only) SOURCE_ONLY=true ;;
    --suite)
      shift
      [ $# -gt 0 ] || { echo "Error: --suite requires argument" >&2; exit 1; }
      SUITE="$1"
      ;;
    *) echo "Error: Unknown option '$1'" >&2; exit 1 ;;
  esac
  shift
done

echo "-------------------------------------"
echo "--- build-ubuntu ---"
echo "-------------------------------------"

if [ -z "$SUITE" ]; then
  SUITE="${UBUNTU_SUITE:-}"
fi
if [ -z "$SUITE" ]; then
  echo "Which Ubuntu suite?"
  select SUITE in $SUITE_OPTIONS; do
    [ -n "$SUITE" ] && break
  done
fi

if [[ ! " $SUITE_OPTIONS " =~ " $SUITE " ]]; then
  echo "Error: Invalid suite '$SUITE'. Use: $SUITE_OPTIONS" >&2
  exit 1
fi

# Version: from configure.ac or env
if [ -n "${UBUNTU_VERSION:-}" ]; then
  VERSION="$UBUNTU_VERSION"
else
  BASE_VER=$(sed -n 's/AC_INIT(\[rsyslog\],\[\([^]]*\)\].*/\1/p' configure.ac | sed 's/\.daily$//')
  TIMESTAMP=$(date -u +%Y%m%d%H%M%S)
  VERSION="${BASE_VER}-local-${TIMESTAMP}"
fi
# Sanitize VERSION: must match package version pattern (digits, dots, hyphens, alphanumeric)
# dpkg requires upstream version to start with a digit
case "$VERSION" in
  *[!0-9A-Za-z.+\~-]*) echo "Error: Invalid VERSION '$VERSION'" >&2; exit 1 ;;
  "") echo "Error: VERSION empty" >&2; exit 1 ;;
  [0-9]*) ;;
  *) echo "Error: VERSION must start with a digit (dpkg requirement): '$VERSION'" >&2; exit 1 ;;
esac
echo "Suite: $SUITE, Version: $VERSION"

# Clean previous packaging artifacts to avoid stale .dsc/.deb from prior runs
rm -rf -- rsyslog-[0-9]*/ rsyslog_*.dsc rsyslog_*.orig.tar.gz rsyslog_*.debian.tar.* *.deb 2>/dev/null || true

# Ensure dpkg-dev and wget (wget needed for jammy/noble qpid-proton fetch)
if ! command -v dpkg-source >/dev/null 2>&1 || ! command -v wget >/dev/null 2>&1; then
  echo "Installing dpkg-dev, wget..."
  if [ "$(id -u)" -eq 0 ]; then
    apt-get update && apt-get install -y dpkg-dev wget
  else
    sudo apt-get update && sudo apt-get install -y dpkg-dev wget
  fi
fi

# Create tarball (uses committed tree only; uncommitted changes are excluded for reproducible builds)
echo "== Create tarball =="
git archive --format=tar.gz --prefix="rsyslog-${VERSION}/" HEAD -o "rsyslog_${VERSION}.orig.tar.gz"

# Prepare source tree
echo "== Prepare source tree =="
tar xf "rsyslog_${VERSION}.orig.tar.gz"
rm -rf "rsyslog-${VERSION}/debian"
cp -r "$DEBIAN_BASE/${SUITE}/v8-stable/debian" "rsyslog-${VERSION}/"
chmod -x "rsyslog-${VERSION}/debian/clean" "rsyslog-${VERSION}/debian/not-installed" 2>/dev/null || true
find "rsyslog-${VERSION}/debian" -type f \( -name '*.dirs' -o -name '*.install.*' -o -name '*.config' -o -name '*.conf.template' -o -name '*.apparmor' -o -name '*.docs' -o -name '*.examples' -o -name '*.logrotate' -o -name '*.triggers' -o -name '*.maintscript' -o -name '*.logcheck.ignore.*' \) -exec chmod -x {} \; 2>/dev/null || true
# Normalize CRLF in copied packaging metadata to avoid dpkg-source parsing failures
# (e.g. "source format '3.0 (quilt)\r' is invalid" on mixed Windows/Linux setups).
find "rsyslog-${VERSION}/debian" -type f -exec sed -i 's/\r$//' {} \;
# Strip executable from plain *.install; keep it for dh-exec scripts (noble uses them)
for f in $(find "rsyslog-${VERSION}/debian" -type f -name '*.install' 2>/dev/null); do
  if head -1 "$f" 2>/dev/null | grep -q '#!/usr/bin/dh-exec'; then
    chmod +x "$f"
  else
    chmod -x "$f"
  fi
done

# Fetch qpid-proton tarball if needed (jammy, noble)
if [ -f "rsyslog-${VERSION}/debian/source/include-binaries" ] && \
   grep -q 'qpid-proton' "rsyslog-${VERSION}/debian/source/include-binaries"; then
  mkdir -p "rsyslog-${VERSION}/debian/qpid-proton"
  if [ ! -f "rsyslog-${VERSION}/debian/qpid-proton/qpid-proton-0.40.0.tar.gz" ]; then
    echo "== Fetch qpid-proton tarball =="
    wget -q -O "rsyslog-${VERSION}/debian/qpid-proton/qpid-proton-0.40.0.tar.gz" \
      https://archive.apache.org/dist/qpid/proton/0.40.0/qpid-proton-0.40.0.tar.gz
    (cd "rsyslog-${VERSION}/debian/qpid-proton" && \
      echo '3e7fe56ca1423f45f71d81f5e1d6ec5f21c073cc580628e12a8dbd545a86805b7312834e0d1234dde43797633d575ed639f21a96239b217500cc0a824482aae3  qpid-proton-0.40.0.tar.gz' | sha512sum -c -)
  fi
fi

# Update changelog
echo "== Update changelog =="
CHANGELOG_VER=$(dpkg-parsechangelog -l "rsyslog-${VERSION}/debian/changelog" -S Version)
CHANGELOG_UPSTREAM="${CHANGELOG_VER%%-*}"
if [ "$CHANGELOG_UPSTREAM" != "$VERSION" ]; then
  CHANGELOG_ESC=$(echo "$CHANGELOG_UPSTREAM" | sed 's/\./\\./g')
  sed -i "1s/${CHANGELOG_ESC}/${VERSION}/" "rsyslog-${VERSION}/debian/changelog"
fi

# Build source package
echo "== Build source package =="
cd "rsyslog-${VERSION}"
dpkg-source -b .
cd ..

echo "Source package built: rsyslog_${VERSION}*.dsc"

if [ "$SOURCE_ONLY" = true ]; then
  echo "Done (source only)."
  exit 0
fi

# Lint (only the just-built .dsc to avoid processing stale artifacts)
# Use || true so lintian findings do not abort the build; fix issues separately.
if command -v lintian >/dev/null 2>&1; then
  echo "== Lint source package =="
  for dsc in "rsyslog_${VERSION}"*.dsc; do
    [ -f "$dsc" ] || continue
    lintian --no-tag-display-limit "$dsc" 2>/dev/null || true
  done
fi

# Build binary (requires docker)
IMAGE=$(get_suite_image "$SUITE")
if ! command -v docker >/dev/null 2>&1; then
  echo "Docker not found. Binary build skipped. Use --source-only to avoid this."
  exit 0
fi

echo "== Build binary package (in $IMAGE) =="
docker run --rm \
  -v "$REPO_ROOT:/build" -w /build \
  -e DEBIAN_FRONTEND=noninteractive \
  -e VERSION="$VERSION" \
  "$IMAGE" bash -c '
    set -e
    apt-get update
    apt-get install -y software-properties-common
    add-apt-repository -y universe
    add-apt-repository --yes --update ppa:adiscon/v8-stable
    apt-get update
    apt-get install -y dpkg-dev devscripts build-essential fakeroot equivs flex wget
    # Build in container-local /tmp to avoid dpkg-deb "control directory has bad permissions 777"
    # when /build is a Windows/NTFS volume mount (Docker Desktop, WSL).
    mkdir -p /tmp/rsyslog-build
    for f in rsyslog_${VERSION}*.dsc rsyslog_${VERSION}*.orig.tar.gz rsyslog_${VERSION}*.debian.tar.*; do
      [ -f "$f" ] && cp -a "$f" /tmp/rsyslog-build/
    done
    cd /tmp/rsyslog-build
    dpkg-source -x rsyslog_${VERSION}*.dsc
    SRCDIR="rsyslog-${VERSION}"
    test -d "$SRCDIR" || (echo "No rsyslog-${VERSION} directory"; ls -la; exit 1)
    cd "$SRCDIR"
    # Fix debhelper config file modes (chmod may not work on Windows host, so do it in container)
    chmod -x debian/clean debian/not-installed 2>/dev/null || true
    find debian -type f \( -name "*.dirs" -o -name "*.install.*" -o -name "*.docs" -o -name "*.examples" -o -name "*.config" -o -name "*.conf.template" -o -name "*.apparmor" -o -name "*.logrotate" -o -name "*.triggers" -o -name "*.maintscript" -o -name "*.logcheck.ignore.*" \) -exec chmod -x {} \;
    # Strip executable from plain *.install; keep it for dh-exec scripts (noble rsyslog.install)
    for f in $(find debian -type f -name "*.install"); do
      if head -1 "$f" 2>/dev/null | grep -q "#!/usr/bin/dh-exec"; then
        chmod +x "$f"
      else
        chmod -x "$f"
      fi
    done
    # Re-fetch qpid-proton tarball in container to avoid corruption from Windows host mount
    if [ -f debian/source/include-binaries ] && grep -q qpid-proton debian/source/include-binaries; then
      mkdir -p debian/qpid-proton
      wget -q -O debian/qpid-proton/qpid-proton-0.40.0.tar.gz \
        https://archive.apache.org/dist/qpid/proton/0.40.0/qpid-proton-0.40.0.tar.gz
      (cd debian/qpid-proton && echo "3e7fe56ca1423f45f71d81f5e1d6ec5f21c073cc580628e12a8dbd545a86805b7312834e0d1234dde43797633d575ed639f21a96239b217500cc0a824482aae3  qpid-proton-0.40.0.tar.gz" | sha512sum -c -)
    fi
    mk-build-deps -i -r -t "apt-get -y --no-install-recommends"
    debuild -b -us -uc
    cp -a ../*.deb /build/
  '

echo "== Built packages =="
find . -maxdepth 2 -name "*.deb" -type f -exec ls -la {} \;
echo "Done."
