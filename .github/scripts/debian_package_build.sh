#!/usr/bin/env bash
set -euo pipefail

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
PY
}

install_prereqs() {
  apt-get update
  apt-get install -y --no-install-recommends \
    autoconf \
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

  local dist_tarball
  dist_tarball="$(ls -1 rsyslog-*.tar.gz | head -1)"
  if [ -z "$dist_tarball" ]; then
    echo "ERROR: make dist did not produce rsyslog-*.tar.gz" >&2
    return 1
  fi

  printf '%s\n' "$dist_tarball"
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

  if [ ! -f "$series_file" ] || [ ! -s "$series_file" ]; then
    return 0
  fi

  cd "$source_dir"
  export QUILT_PATCHES=debian/patches

  while true; do
    quilt pop -a >/dev/null 2>&1 || true

    set +e
    quilt push -a > /tmp/quilt-push.log 2>&1
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
