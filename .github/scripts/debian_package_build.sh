#!/usr/bin/env bash
set -euo pipefail

UPSTREAM_PATCH_DETECT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/devtools/release/upstream_patch_detect.py"

append_finding() {
  local findings_dir="$1"
  local section="$2"
  local message="$3"

  mkdir -p "$findings_dir"
  printf '%s\n' "$message" >> "$findings_dir/$section.md"
}

load_policy() {
  local policy_file="$1"
  local out_dir="$2"

  rm -rf "$out_dir"
  mkdir -p "$out_dir"

  python3 - "$policy_file" "$out_dir" <<'PY'
import json
import os
import sys

policy_path, out_dir = sys.argv[1], sys.argv[2]
with open(policy_path, "r", encoding="utf-8") as fh:
    policy = json.load(fh)

sections = {
    "allowed_patch_skips": ("patch", "reason"),
    "supplemental_build_deps": ("package", "reason"),
    "not_installed_paths": ("path", "reason"),
}

for key, (name_key, reason_key) in sections.items():
    names_path = os.path.join(out_dir, f"{key}.txt")
    reasons_path = os.path.join(out_dir, f"{key}.reasons.txt")
    with open(names_path, "w", encoding="utf-8") as names_fh, \
            open(reasons_path, "w", encoding="utf-8") as reasons_fh:
        for entry in policy.get(key, []):
            name = entry.get(name_key, "").strip()
            reason = entry.get(reason_key, "").strip()
            if not name:
                continue
            names_fh.write(f"{name}\n")
            reasons_fh.write(f"{name}\t{reason}\n")

# Copy reviewed refresh content into the loaded policy so later resolution does
# not depend on the caller's working directory. Native patch hashes remain the
# authority: a changed distro patch must be reviewed again.
refreshes = []
for entry in policy.get("patch_refreshes", []):
    replacement = os.path.realpath(os.path.join(os.path.dirname(policy_path), entry["replacement"]))
    policy_dir = os.path.realpath(os.path.dirname(policy_path))
    if os.path.commonpath([replacement, policy_dir]) != policy_dir:
        raise ValueError("patch refresh replacement escapes policy directory")
    with open(replacement, encoding="utf-8") as fh:
        refreshes.append(dict(entry, content=fh.read()))
with open(os.path.join(out_dir, "patch_refreshes.json"), "w", encoding="utf-8") as fh:
    json.dump(refreshes, fh)

replacements_path = os.path.join(out_dir, "control_replacements.tsv")
replacements_reasons_path = os.path.join(out_dir, "control_replacements.reasons.txt")
with open(replacements_path, "w", encoding="utf-8") as replacements_fh, \
        open(replacements_reasons_path, "w", encoding="utf-8") as reasons_fh:
    for entry in policy.get("control_replacements", []):
        pattern = entry.get("pattern", "").strip()
        replacement = entry.get("replacement", "").strip()
        reason = entry.get("reason", "").strip()
        if not pattern:
            continue
        replacements_fh.write(f"{pattern}\t{replacement}\n")
        reasons_fh.write(f"{pattern}\t{reason}\n")

manifest_path = os.path.join(out_dir, "install_manifest_additions.tsv")
manifest_reasons_path = os.path.join(out_dir, "install_manifest_additions.reasons.txt")
with open(manifest_path, "w", encoding="utf-8") as manifest_fh, \
        open(manifest_reasons_path, "w", encoding="utf-8") as reasons_fh:
    for entry in policy.get("install_manifest_additions", []):
        manifest = entry.get("manifest", "").strip()
        path = entry.get("path", "").strip()
        reason = entry.get("reason", "").strip()
        if not manifest or not path:
            continue
        manifest_fh.write(f"{manifest}\t{path}\n")
        reasons_fh.write(f"{manifest}:{path}\t{reason}\n")
PY
}

install_prereqs() {
  apt-get update
  apt-get install -y --no-install-recommends \
    autoconf \
    autoconf-archive \
    automake \
    autotools-dev \
    bison \
    build-essential \
    ca-certificates \
    devscripts \
    dpkg-dev \
    equivs \
    flex \
    g++ \
    git \
    libtool \
    libdistro-info-perl \
    libyaml-dev \
    make \
    pkg-config \
    python3 \
    python3-docutils \
    quilt
}

fetch_debian_packaging() {
  local repo_url="$1"
  local branch="$2"
  local target_dir="$3"
  local clone_dir="/tmp/debian-packaging-clone"

  rm -rf "$target_dir" "$clone_dir"
  git clone --depth=1 --branch "$branch" \
    "$repo_url" \
    "$clone_dir"

  if [ ! -d "$clone_dir/debian" ]; then
    echo "ERROR: Could not obtain Debian packaging files from $repo_url:$branch" >&2
    return 1
  fi

  cp -r "$clone_dir/debian" "$target_dir"
}

enable_debian_source_repositories() {
  local deb822_files=(/etc/apt/sources.list.d/*.sources)

  if [ -e "${deb822_files[0]}" ]; then
    python3 - "${deb822_files[@]}" <<'PY'
import pathlib
import sys

for name in sys.argv[1:]:
    path = pathlib.Path(name)
    lines = path.read_text(encoding="utf-8").splitlines()
    updated = []
    for line in lines:
        if line.startswith("Types:"):
            types = line.removeprefix("Types:").split()
            if "deb" in types and "deb-src" not in types:
                types.append("deb-src")
            line = f"Types: {' '.join(types)}"
        updated.append(line)
    path.write_text("\n".join(updated) + "\n", encoding="utf-8")
PY
  elif [ -f /etc/apt/sources.list ]; then
    sed -n 's/^deb[[:space:]]\+/deb-src /p' /etc/apt/sources.list \
      > /etc/apt/sources.list.d/rsyslog-deb-src.list
  fi

  apt-get update
  apt-cache showsrc rsyslog >/dev/null 2>&1 || {
    echo "ERROR: target Debian release does not provide rsyslog source metadata" >&2
    return 1
  }
}

fetch_debian_release_packaging() {
  local target_dir="$1"
  local version_file="$2"
  local source_root
  local source_dir
  local packaging_version

  source_root="$(mktemp -d)"
  rm -rf "$target_dir"

  enable_debian_source_repositories
  (
    cd "$source_root"
    apt-get source --only-source rsyslog
  )

  source_dir="$(
    find "$source_root" -mindepth 1 -maxdepth 1 -type d \
      -name 'rsyslog-*' -print -quit
  )"
  if [ -z "$source_dir" ] || [ ! -d "$source_dir/debian" ]; then
    echo "ERROR: could not extract the target Debian release's rsyslog source package" >&2
    return 1
  fi

  packaging_version="$(
    dpkg-parsechangelog -l "$source_dir/debian/changelog" -S Version
  )"
  cp -r "$source_dir/debian" "$target_dir"
  printf '%s\n' "$packaging_version" > "$version_file"
  rm -rf "$source_root"
}

apply_control_replacements() {
  local control_file="$1"
  local replacements_file="$2"
  local reasons_file="$3"
  local findings_dir="${4:-}"

  [ -s "$replacements_file" ] || return 0
  [ -f "$control_file" ] || {
    echo "ERROR: missing Debian control file: $control_file" >&2
    return 1
  }

  python3 - "$control_file" "$replacements_file" <<'PY'
import re
import sys

control_path, replacements_path = sys.argv[1:3]
with open(control_path, "r", encoding="utf-8") as fh:
    control = fh.read()

with open(replacements_path, "r", encoding="utf-8") as fh:
    for line in fh:
        line = line.rstrip("\n")
        if not line:
            continue
        pattern, replacement = line.split("\t", 1)
        control = re.sub(pattern, replacement, control)

with open(control_path, "w", encoding="utf-8") as fh:
    fh.write(control)
PY

  if [ -n "$findings_dir" ]; then
    record_policy_items "$reasons_file" "$findings_dir" \
      "allowed_packaging_drift" "Update Debian control replacement for"
  fi
}

apply_install_manifest_additions() {
  local packaging_dir="$1"
  local additions_file="$2"
  local manifest
  local path
  local target

  [ -s "$additions_file" ] || return 0

  while IFS=$'\t' read -r manifest path; do
    [ -n "$manifest" ] && [ -n "$path" ] || continue
    case "$manifest" in
      */*|.*)
        echo "ERROR: invalid Debian install manifest name: $manifest" >&2
        return 1
        ;;
    esac
    target="$packaging_dir/$manifest"
    [ -f "$target" ] || {
      echo "ERROR: missing Debian install manifest: $target" >&2
      return 1
    }
    if ! grep -Fqx "$path" "$target"; then
      printf '%s\n' "$path" >> "$target"
    fi
  done < "$additions_file"
}

run_dist_build() {
  local workspace="$1"

  cd "$workspace"
  autoreconf -fvi
  ./configure --enable-silent-rules --disable-testbench \
     --disable-imdiag --disable-imdocker --disable-imfile \
     --disable-default-tests --disable-impstats \
     --disable-impstats-push --disable-imptcp \
     --disable-mmanon --disable-mmaudit --disable-mmfields \
     --disable-mmjsonparse --disable-mmpstrucdata \
     --disable-mmsequence --disable-mmutf8fix --disable-mail \
     --disable-omprog --disable-improg --disable-omruleset \
     --disable-omstdout --disable-omuxsock \
     --disable-pmaixforwardedfrom --disable-pmciscoios \
     --disable-pmcisconames --disable-pmlastmsg --disable-pmsnare \
     --disable-libgcrypt --disable-mmnormalize \
     --disable-omudpspoof --disable-relp --disable-mmsnmptrapd \
     --disable-gnutls --disable-usertools --disable-mysql \
     --disable-valgrind --disable-omjournal --enable-libsystemd \
     --disable-mmkubernetes --disable-imjournal \
     --disable-omkafka --disable-imkafka --disable-ommongodb \
     --disable-omrabbitmq --disable-journal-tests --disable-mmdarwin \
     --disable-helgrind --disable-uuid --disable-fmhttp
  make dist
}

find_dist_tarball() {
  local workspace="$1"

  cd "$workspace"

  local dist_tarballs=(rsyslog-*.tar.gz)
  if [ ${#dist_tarballs[@]} -eq 0 ] || [ ! -e "${dist_tarballs[0]}" ]; then
    echo "ERROR: make dist did not produce rsyslog-*.tar.gz" >&2
    return 1
  fi

  printf '%s\n' "${dist_tarballs[0]}"
}

unpack_source_tree() {
  local workspace="$1"
  local dist_tarball="$2"
  local packaging_dir="$3"
  local source_dir="$4"

  rm -rf "$source_dir"
  mkdir -p "$source_dir"
  tar -xf "$workspace/$dist_tarball" -C "$source_dir" --strip-components=1
  cp -r "$packaging_dir" "$source_dir/debian"
}

install_build_deps() {
  local control_file="$1"
  local extra_deps_file="$2"
  local findings_dir="${3:-}"

  if [ -s "$extra_deps_file" ]; then
    while IFS= read -r pkg; do
      [ -n "$pkg" ] || continue
      if ! apt-cache show "$pkg" >/dev/null 2>&1; then
        if [ -n "$findings_dir" ]; then
          append_finding "$findings_dir" "unavailable_dependencies" \
            "- Policy-listed package \`$pkg\` is not available in the Debian experimental baseline environment."
        fi
        echo "ERROR: policy-listed package '$pkg' is not available in the Debian experimental baseline environment" >&2
        return 1
      fi
    done < "$extra_deps_file"

    xargs -r apt-get install -y --no-install-recommends < "$extra_deps_file"
  fi

  local apt_tool
  apt_tool="apt-get -o Debug::pkgProblemResolver=yes"
  apt_tool="$apt_tool --no-install-recommends --yes"
  mk-build-deps --install --remove \
    --tool="$apt_tool" \
    "$control_file"
}

record_policy_items() {
  local reasons_file="$1"
  local findings_dir="$2"
  local primary_section="$3"
  local release_prefix="$4"

  [ -s "$reasons_file" ] || return 0

  while IFS=$'\t' read -r name reason; do
    [ -n "$name" ] || continue
    append_finding "$findings_dir" "$primary_section" \
      "- \`$name\`: ${reason:-No reason recorded.}"
    append_finding "$findings_dir" "release_handoff_items" \
      "- $release_prefix \`$name\`: ${reason:-No reason recorded.}"
  done < "$reasons_file"
}

apply_not_installed_policy() {
  local source_dir="$1"
  local not_installed_file="$2"
  local reasons_file="$3"
  local findings_dir="${4:-}"
  local not_installed_path="$source_dir/debian/not-installed"

  [ -s "$not_installed_file" ] || return 0

  touch "$not_installed_path"
  while IFS= read -r entry; do
    [ -n "$entry" ] || continue
    if ! grep -Fqx "$entry" "$not_installed_path"; then
      printf '%s\n' "$entry" >> "$not_installed_path"
    fi
  done < "$not_installed_file"

  if [ -n "$findings_dir" ]; then
    record_policy_items "$reasons_file" "$findings_dir" \
      "allowed_packaging_drift" "Update Debian not-installed handling for"
  fi
}

apply_patch_refreshes() {
  local source_dir="$1"
  local refreshes_file="$2"

  [ -f "$refreshes_file" ] || return 0
  python3 - "$source_dir" "$refreshes_file" <<'PY_REFRESH'
import hashlib
import json
import pathlib
import re
import sys

source_dir, manifest = sys.argv[1:]
patch_dir = pathlib.Path(source_dir) / "debian/patches"
with open(manifest, encoding="utf-8") as fh:
    entries = json.load(fh)
series = patch_dir / "series"
active = set()
if series.exists():
    for line in series.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            active.add(line.split()[0])
updates = []
for entry in entries:
    name = entry["patch"]
    if pathlib.PurePosixPath(name).name != name or name in ("", ".", ".."):
        raise ValueError("invalid patch refresh name")
    if name not in active:
        raise ValueError("reviewed Debian patch no longer in series: " + name)
    expected = entry["sha256"]
    if not re.fullmatch(r"[0-9a-f]{64}", expected) or not entry.get("reason", "").strip():
        raise ValueError("patch refresh requires SHA-256 and reason")
    patch = patch_dir / name
    # An absent or altered native patch is baseline drift, never an implicit skip.
    actual = hashlib.sha256(patch.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError("native Debian patch changed; review refresh: " + name)
    updates.append((patch, entry["content"], entry["reason"]))
# Check every guard before modifying any patch.
for patch, content, reason in updates:
    patch.write_text(content, encoding="utf-8")
    print("Refreshed reviewed Debian patch " + patch.name + ": " + reason)
PY_REFRESH
}

resolve_patch_policy() {
  local source_dir="$1"
  local allowed_file="$2"
  local reasons_file="$3"
  local mode="$4"
  local findings_dir="${5:-}"
  local allowed_tmp="/tmp/debian-ci-allowed-patches.txt"
  local excluded_file="/tmp/excluded-debian-patches.txt"
  local series_file="$source_dir/debian/patches/series"

  : > "$excluded_file"
  cp "$allowed_file" "$allowed_tmp" 2>/dev/null || : > "$allowed_tmp"

  local refresh_error
  if ! refresh_error="$(apply_patch_refreshes "$source_dir" "$(dirname "$allowed_file")/patch_refreshes.json" 2>&1)"; then
    printf '%s\n' "$refresh_error" >&2
    if [ "$mode" = "diagnostics" ] && [ -n "$findings_dir" ]; then
      append_finding "$findings_dir" "blocking_gate_failures" \
        "- Reviewed Debian patch refresh failed; native baseline requires review."
      append_finding "$findings_dir" "release_handoff_items" \
        "- Review Debian patch refresh policy against the changed native baseline."
      return 0
    fi
    return 1
  fi
  [ -z "$refresh_error" ] || printf '%s\n' "$refresh_error"

  if [ ! -f "$series_file" ] || [ ! -s "$series_file" ]; then
    return 0
  fi

  cd "$source_dir"
  export QUILT_PATCHES=debian/patches

  while true; do
    quilt pop -a >/dev/null 2>&1 || true

    set +e
    quilt push --fuzz=0 -a > /tmp/quilt-push.log 2>&1
    rc=$?
    set -e
    cat /tmp/quilt-push.log

    if grep -q "No series file found" /tmp/quilt-push.log; then
      break
    fi

    if [ "$rc" -eq 0 ]; then
      break
    fi

    local failed_patch
    local failed_patch_name
    local excluded_key
    failed_patch="$(
      sed -n 's/^Applying patch \(.*\)$/\1/p' /tmp/quilt-push.log | tail -1
    )"
    if [ -z "$failed_patch" ]; then
      echo "ERROR: could not determine failing patch" >&2
      return 1
    fi
    failed_patch_name="${failed_patch##*/}"

    # A distribution backport of a commit that the packaged upstream already
    # contains cannot apply and is not needed; skip it and report it.
    local evidence
    if ! grep -Fxq "$failed_patch" "$allowed_tmp" && \
       ! grep -Fxq "$failed_patch_name" "$allowed_tmp" && \
       [ -n "${RSYSLOG_UPSTREAM_COMMIT_INDEX:-}" ] && \
       evidence="$(python3 "$UPSTREAM_PATCH_DETECT" "$RSYSLOG_UPSTREAM_COMMIT_INDEX" \
         "$QUILT_PATCHES/$failed_patch_name")"; then
      echo "::warning title=Debian patch already upstream::skipped $failed_patch_name: $evidence; add it to allowed_patch_skips after review"
      printf '%s\n' "$failed_patch_name" >> "$allowed_tmp"
    fi

    if ! grep -Fxq "$failed_patch" "$allowed_tmp" && \
       ! grep -Fxq "$failed_patch_name" "$allowed_tmp"; then
      if [ "$mode" = "diagnostics" ] && [ -n "$findings_dir" ]; then
        append_finding "$findings_dir" "blocking_gate_failures" \
          "- Unexpected Debian patch failure: \`$failed_patch_name\` does not match the allowlist."
        append_finding "$findings_dir" "release_handoff_items" \
          "- Refresh or drop Debian patch \`$failed_patch_name\`; CI could not apply it against upstream."
        quilt pop -a >/dev/null 2>&1 || true
        return 0
      fi

      echo "ERROR: unexpected Debian patch failure '$failed_patch_name'" >&2
      return 1
    fi

    excluded_key="$failed_patch"
    if grep -Fxq "$excluded_key" "$excluded_file"; then
      echo "ERROR: patch resolution loop detected at '$failed_patch_name'" >&2
      return 1
    fi

    awk -v p="$failed_patch" -v pb="$failed_patch_name" '
      /^#/ {print; next}
      NF == 0 {print; next}
      $1 == p || $1 == pb {removed = 1; next}
      {print}
      END {if (!removed) exit 9}
    ' "$series_file" > "$series_file.new"
    mv "$series_file.new" "$series_file"
    printf '%s\n' "$excluded_key" >> "$excluded_file"

    if [ -n "$findings_dir" ]; then
      local reason
      reason="$(
        awk -F '\t' -v p="$failed_patch" -v pb="$failed_patch_name" '$1 == p || $1 == pb {print $2}' "$reasons_file"
      )"
      append_finding "$findings_dir" "allowed_patch_drift" \
        "- Skipped allowed Debian patch \`$failed_patch_name\`: ${reason:-No reason recorded.}"
      append_finding "$findings_dir" "release_handoff_items" \
        "- Refresh or drop Debian patch \`$failed_patch_name\`: ${reason:-No reason recorded.}"
    fi
  done

  quilt pop -a >/dev/null 2>&1 || true
}

check_docs_build_log() {
  local build_log="$1"

  [ -f "$build_log" ] || return 0

  if grep -Eq '(^|/)(doc/source|source)/[^:]+:[0-9]+: (CRITICAL|ERROR):|^Sphinx error:' "$build_log"; then
    echo "ERROR: Debian docs build emitted fatal Sphinx/docutils diagnostics" >&2
    return 1
  fi
}

build_package() {
  local source_dir="$1"
  local build_log="${2:-/tmp/dpkg-buildpackage.log}"
  local rc

  cd "$source_dir"
  export DEB_BUILD_OPTIONS="nocheck"

  set +e
  dpkg-buildpackage -b -uc -us -j"$(nproc)" 2>&1 | tee "$build_log"
  rc=${PIPESTATUS[0]}
  set -e

  if [ "$rc" -ne 0 ]; then
    return "$rc"
  fi

  check_docs_build_log "$build_log"
}

verify_docs() {
  local source_dir="$1"

  [ -d "$source_dir/debian/build" ] && \
  [ -f "$source_dir/debian/build/index.html" ]
}

init_findings_dir() {
  local findings_dir="$1"
  rm -rf "$findings_dir"
  mkdir -p "$findings_dir"
}

render_findings() {
  local findings_dir="$1"
  local output_file="$2"
  local gate_result="${3:-unknown}"
  local title="${4:-Debian diagnostics findings}"

  cat > "$output_file" <<EOF2
${title}
==========================

Required gate result: $gate_result
EOF2

  local section title_line
  while IFS='|' read -r section title_line; do
    if [ -s "$findings_dir/$section.md" ]; then
      printf '\n%s\n\n' "$title_line" >> "$output_file"
      cat "$findings_dir/$section.md" >> "$output_file"
    fi
  done <<'EOF2'
blocking_gate_failures|Blocking gate failures
allowed_patch_drift|Allowed Debian patch drift
allowed_dependency_drift|Allowed Debian dependency drift
allowed_packaging_drift|Allowed packaging drift
unavailable_dependencies|Unavailable Debian experimental baseline dependencies
docs_test_observations|Debian docs/test observations
release_handoff_items|Release handoff items
EOF2
}

case "${1:-}" in
  append_finding) shift; append_finding "$@" ;;
  load_policy) shift; load_policy "$@" ;;
  install_prereqs) shift; install_prereqs "$@" ;;
  fetch_debian_packaging) shift; fetch_debian_packaging "$@" ;;
  fetch_debian_release_packaging) shift; fetch_debian_release_packaging "$@" ;;
  apply_control_replacements) shift; apply_control_replacements "$@" ;;
  apply_install_manifest_additions) shift; apply_install_manifest_additions "$@" ;;
  run_dist_build) shift; run_dist_build "$@" ;;
  find_dist_tarball) shift; find_dist_tarball "$@" ;;
  unpack_source_tree) shift; unpack_source_tree "$@" ;;
  install_build_deps) shift; install_build_deps "$@" ;;
  record_policy_items) shift; record_policy_items "$@" ;;
  apply_not_installed_policy) shift; apply_not_installed_policy "$@" ;;
  resolve_patch_policy) shift; resolve_patch_policy "$@" ;;
  check_docs_build_log) shift; check_docs_build_log "$@" ;;
  build_package) shift; build_package "$@" ;;
  verify_docs) shift; verify_docs "$@" ;;
  init_findings_dir) shift; init_findings_dir "$@" ;;
  render_findings) shift; render_findings "$@" ;;
  *)
    echo "Usage: $0 <command> [args...]" >&2
    exit 2
    ;;
esac
