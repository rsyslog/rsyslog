#!/bin/bash
# Build RPMs via mock. Migrated from rpmmaker.sh.
# Usage: ./build-rpms.sh [--build-only]
#   --build-only: skip signing and repo copy; output RPMs to build-result/
# Env: RPM_SPEC, RPM_PLATFORM, RPM_ARCH (RPM_REPO only when not --build-only)
#       GPG_PASSPHRASE (optional): for repo signing; if unset, uses passfile.txt (must not be committed)

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

BUILD_ONLY=false
for arg in "$@"; do
  case "$arg" in
    --build-only) BUILD_ONLY=true ;;
  esac
done

echo "-------------------------------------"
echo "--- build-rpms ---"
echo "-------------------------------------"

if [ -z "${RPM_SPEC:-}" ]; then
  echo "SPEC Filebasename?"
  select szSpec in $SPECOPTIONS; do
    echo "Making RPM for '$szSpec.spec'"
    break
  done
else
  echo "SPEC is set to '$RPM_SPEC'"
  szSpec=$RPM_SPEC
fi

if [ -z "${RPM_PLATFORM:-}" ]; then
  echo "Which Linux Platform?"
  select szDist in $PLATOPTIONS "All"; do
    case "$szDist" in "All") szDist=$PLATOPTIONS ;; esac
    echo "Making RPM for '$szDist'"
    break
  done
else
  echo "PLATFORM is set to '$RPM_PLATFORM'"
  szDist=$RPM_PLATFORM
fi

if [ -z "${RPM_ARCH:-}" ]; then
  echo "Which Architecture?"
  select szArch in $ARCHOPTIONS "All"; do
    case "$szArch" in "All") szArch=$ARCHOPTIONS ;; esac
    echo "Making RPM for '$szArch'"
    break
  done
else
  echo "ARCH is set to '$RPM_ARCH'"
  szArch=$RPM_ARCH
fi

if [ "$BUILD_ONLY" = true ] || [ -z "$szYumRepoDir" ]; then
  szSubRepo=""
  BUILD_RESULT_DIR="$szBaseDir/build-result"
  mkdir -p "$BUILD_RESULT_DIR"
else
  if [ -z "${RPM_REPO:-}" ]; then
    echo "Which YUM repository?"
    select szSubRepo in $REPOOPTIONS "All"; do
      case "$szSubRepo" in "All") szSubRepo=$REPOOPTIONS ;; esac
      break
    done
  else
    echo "REPO is set to '$RPM_REPO'"
    szSubRepo=$RPM_REPO
  fi
fi

# Download source tarball if missing (from official URLs in spec files)
# Uses temp file to avoid leaving corrupt partial downloads that future runs would treat as valid.
_download_source() {
  local spec="$1" name="$2" version="$3" url="$4" filename="$5"
  filename="$(basename "$filename")"
  local tarball="$szRpmBaseDir/SOURCES/$filename"
  if [ ! -f "$tarball" ]; then
    echo "Downloading $filename (not found in SOURCES)"
    local tmp="$tarball.$$.tmp"
    if command -v wget >/dev/null 2>&1; then
      wget -q -O "$tmp" "$url" || { rm -f "$tmp"; echo "ERROR: Failed to download $filename"; return 1; }
    else
      curl -fsSL -o "$tmp" "$url" || { rm -f "$tmp"; echo "ERROR: Failed to download $filename"; return 1; }
    fi
    mv "$tmp" "$tarball"
    echo "Downloaded $tarball"
  fi
}

_spec_path="$szRpmBaseDir/SPECS/$szSpec.spec"
if [ -f "$_spec_path" ]; then
  _pkg_name=$(grep -E '^Name:' "$_spec_path" | awk '{print $2}')
  _version=$(grep -E '^Version:' "$_spec_path" | awk '{print $2}')
  case "$_pkg_name" in
    rsyslog)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://www.rsyslog.com/files/download/rsyslog/rsyslog-${_version}.tar.gz" \
        "rsyslog-${_version}.tar.gz" || exit 1
      ;;
    libestr)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://libestr.adiscon.com/files/download/libestr-${_version}.tar.gz" \
        "libestr-${_version}.tar.gz" || exit 1
      ;;
    libfastjson4)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://github.com/rsyslog/libfastjson/archive/refs/tags/v${_version}.tar.gz" \
        "v${_version}.tar.gz" || exit 1
      ;;
    librelp)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://download.rsyslog.com/librelp/librelp-${_version}.tar.gz" \
        "librelp-${_version}.tar.gz" || exit 1
      ;;
    liblognorm5)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://www.liblognorm.com/files/download/liblognorm-${_version}.tar.gz" \
        "liblognorm-${_version}.tar.gz" || exit 1
      ;;
    liblogging)
      _download_source "$_spec_path" "$_pkg_name" "$_version" \
        "https://download.rsyslog.com/liblogging/liblogging-${_version}.tar.gz" \
        "liblogging-${_version}.tar.gz" || exit 1
      ;;
    *)
      echo "No auto-download for $_pkg_name (sources must exist in SOURCES)"
      ;;
  esac
fi

for distro in $szDist; do
  for arch in $szArch; do
    echo "Making Source RPM for $szSpec.spec in $distro-$arch"
    if sudo mock --verbose -r "$distro-$arch" --buildsrpm --spec "$szRpmBaseDir/SPECS/$szSpec.spec" --sources "$szRpmBaseDir/SOURCES"; then
      echo "mock rpm buildsrpm succeeded"
      shopt -s nullglob
      szSrcDirFiles=(/var/lib/mock/"$distro-$arch"/result/*.src.rpm)
      shopt -u nullglob
      if [ "${#szSrcDirFiles[@]}" -ne 1 ]; then
        echo "Error: Expected 1 SRPM, but found ${#szSrcDirFiles[@]}" >&2
        exit 1
      fi
      szSrcDirFile="${szSrcDirFiles[0]}"
      szSrcFile=$(basename "$szSrcDirFile")
      echo "Making RPMs from sourcefile '$szSrcDirFile'"
      mkdir -p "$szRpmBuildDir"
      sudo mv "$szSrcDirFile" "$szRpmBuildDir/"
      if sudo mock -r "$distro-$arch" "$szRpmBuildDir/$szSrcFile"; then
        echo "mock rpm build succeeded"
        sudo chown "$szLocalUser" /var/lib/mock/"$distro-$arch"/result/*.rpm
        if [ "$BUILD_ONLY" = true ] || [ -z "$szYumRepoDir" ]; then
          cp /var/lib/mock/"$distro-$arch"/result/*.rpm "$BUILD_RESULT_DIR/"
          echo "RPMs copied to $BUILD_RESULT_DIR"
          sudo rm -f /var/lib/mock/"$distro-$arch"/result/*.rpm
        else
          if sudo rpm --addsign /var/lib/mock/"$distro-$arch"/result/*.rpm; then
            for subrepo in $szSubRepo; do
              repo=$szYumRepoDir/$subrepo/$distro/$arch
              sudo cp /var/lib/mock/"$distro-$arch"/result/*.rpm "$repo/RPMS/"
              echo "Copying RPMs to $repo"
              sudo createrepo -q -s sha256 -o "$repo" -d -p "$repo"
              sudo rm -f "$repo/repodata/repomd.xml.asc"
              # Prefer GPG_PASSPHRASE env var (e.g. CI secrets); fallback to passfile.txt (must not be committed)
              # GnuPG 2.1+ requires --batch and --pinentry-mode loopback for non-interactive passphrase input
              if [ -n "${GPG_PASSPHRASE:-}" ]; then
                echo "$GPG_PASSPHRASE" | sudo gpg --batch --pinentry-mode loopback --passphrase-fd 0 --detach-sign --armor "$repo/repodata/repomd.xml"
              else
                sudo gpg --batch --pinentry-mode loopback --passphrase-file passfile.txt --detach-sign --armor "$repo/repodata/repomd.xml"
              fi
            done
            echo "Cleaning up RPMs"
            sudo rm -f /var/lib/mock/"$distro-$arch"/result/*.rpm
          else
            echo "rpmsign FAILED"
            exit 1
          fi
        fi
      else
        echo "mock rpm build FAILED"
        exit 1
      fi
    else
      echo "mock rpm buildsrpm FAILED"
      exit 1
    fi
  done
done

echo "Done."
exit 0
