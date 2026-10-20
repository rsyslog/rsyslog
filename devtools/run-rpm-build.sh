#!/bin/bash
# CI helper: build RPMs from current source via mock.
# Run from repo root. Expects mock configs in /etc/mock/, epel-release and mock installed.
# Env: MOCK_CONFIG (default: epel-9-x86_64)
#
# Steps: autoreconf, configure, make dist; patch spec for local tarball; mock --buildsrpm; mock build.

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
cd "$REPO_ROOT"

MOCK_CONFIG="${MOCK_CONFIG:-epel-9-x86_64}"
RPM_SPEC="${RPM_SPEC:-rsyslog-v8-stable}"
RPM_DIR="$REPO_ROOT/packaging/rpm"
RPM_SOURCES="$RPM_DIR/rpmbuild/SOURCES"
RPM_SPECS="$RPM_DIR/rpmbuild/SPECS"
BUILD_RESULT="$REPO_ROOT/build-result"
CI_SPEC="$BUILD_RESULT/rsyslog-ci.spec"

# Heartbeat so the job shows progress (spec %prep can take 30+ min)
heartbeat() {
  while true; do
    sleep 120
    echo "[$(date -u +%H:%M:%S)] CI still building (mock chroot/build in progress)..."
  done
}
heartbeat &
HEARTBEAT_PID=$!

# Diagnostics for intermittent "Curl error (7): Could not connect to server"
# failures against the Adiscon repositories inside mock. Prints name
# resolution plus IPv4/IPv6 connect timing for each repository host so a
# failing run shows whether the host was unreachable, refused, or resolved
# to an address family the runner cannot route. Never fails the build.
NETPROBE_HOSTS="download.adiscon.com rpms.adiscon.com"
NETPROBE_URLS="https://download.adiscon.com/rpms/RPM-GPG-KEY-Adiscon https://rpms.adiscon.com/v8-stable/"
netprobe() {
  local host url fam
  echo "=== netprobe ($1) $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  for host in $NETPROBE_HOSTS; do
    echo "--- getent ahosts $host"
    getent ahosts "$host" | awk '{print $1}' | sort -u | grep . || echo "(resolution failed)"
  done
  for url in $NETPROBE_URLS; do
    for fam in -4 -6; do
      echo "--- curl $fam $url"
      curl "$fam" -sS -o /dev/null --max-time 20 \
        -w 'http=%{http_code} ip=%{remote_ip} connect=%{time_connect}s tls=%{time_appconnect}s total=%{time_total}s\n' \
        "$url" 2>&1 || true
    done
  done
  echo "=== end netprobe ==="
}
on_exit() {
  rc=$?
  kill $HEARTBEAT_PID 2>/dev/null || true
  if [ "$rc" -ne 0 ]; then
    netprobe "after failure, exit $rc"
  fi
}
trap on_exit EXIT

echo "== Step 1: autoreconf, configure, make dist =="
autoreconf -fvi
./configure --enable-silent-rules
make dist

echo "== Step 2: extract version and prepare SOURCES =="
shopt -s nullglob
TARBALLS=(rsyslog-*.tar.gz)
shopt -u nullglob
if [ "${#TARBALLS[@]}" -ne 1 ]; then
  echo "Error: Expected 1 tarball, but found ${#TARBALLS[@]}." >&2
  ls -l rsyslog-*.tar.gz 2>/dev/null || true
  exit 1
fi
TARBALL="${TARBALLS[0]}"
VERSION="${TARBALL#rsyslog-}"
VERSION="${VERSION%.tar.gz}"
if [[ ! "$VERSION" =~ ^[0-9][0-9.a-zA-Z_-]*$ ]]; then
  echo "Error: Invalid version string: $VERSION" >&2
  exit 1
fi
echo "Version: $VERSION"
cp -v "$TARBALL" "$RPM_SOURCES/"

echo "== Step 3: create CI spec (local tarball) =="
mkdir -p "$BUILD_RESULT"
cp "$RPM_SPECS/${RPM_SPEC}.spec" "$CI_SPEC"
sed -i -r \
    -e "s/^(Version:\s*).*/\1$VERSION/" \
    -e "s/^(Source0:\s*).*/\1%{name}-%{version}.tar.gz/" \
    "$CI_SPEC"

netprobe "before mock"

echo "== Step 4: mock --buildsrpm =="
if ! mock -r "$MOCK_CONFIG" --verbose --buildsrpm \
  --spec "$CI_SPEC" \
  --sources "$RPM_SOURCES" \
  --resultdir "$BUILD_RESULT/srpm"; then
  echo "=== mock --buildsrpm FAILED, logs ==="
  tail -300 "$BUILD_RESULT/srpm/"*.log 2>/dev/null || true
  find /var/lib/mock -name "*.log" -exec tail -200 {} \; 2>/dev/null || true
  exit 1
fi

echo "== Step 5: mock build from SRPM =="
shopt -s nullglob
SRPMS=("$BUILD_RESULT/srpm/"*.src.rpm)
shopt -u nullglob
if [ "${#SRPMS[@]}" -ne 1 ]; then
  echo "Error: Expected 1 SRPM, but found ${#SRPMS[@]}." >&2
  ls -l "$BUILD_RESULT/srpm/"*.src.rpm 2>/dev/null || true
  exit 1
fi
SRPM="${SRPMS[0]}"
if ! mock -r "$MOCK_CONFIG" --verbose "$SRPM" --resultdir "$BUILD_RESULT/rpms"; then
  echo "=== mock build FAILED, logs ==="
  tail -300 "$BUILD_RESULT/rpms/"*.log 2>/dev/null || true
  find /var/lib/mock -name "*.log" -exec tail -200 {} \; 2>/dev/null || true
  exit 1
fi

echo "== Step 6: collect artifacts =="
cp -v "$SRPM" "$BUILD_RESULT/"
cp -v "$BUILD_RESULT/rpms/"*.rpm "$BUILD_RESULT/"
ls -la "$BUILD_RESULT/"
echo "Done."
