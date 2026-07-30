#!/usr/bin/env bash
# Verify the package contract after installing the base and module packages.
set -euo pipefail

rsyslogd_bin="${RSYSLOGD_BIN:-rsyslogd}"
module_args=()
if [ -n "${RSYSLOG_MODULE_PATH:-}" ]; then
	module_args=(-M "$RSYSLOG_MODULE_PATH")
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

cat > "$tmp_dir/base.yaml" <<'EOF'
version: 2
global:
  preserveFQDN: "off"
rulesets:
  - name: main
    script: |
      action(type="omfile" file="/dev/null")
EOF

cat > "$tmp_dir/omazuredce.yaml" <<'EOF'
version: 2
modules:
  - load: omazuredce
rulesets:
  - name: main
    script: |
      action(type="omfile" file="/dev/null")
EOF

"$rsyslogd_bin" -N1 -f "$tmp_dir/base.yaml" "${module_args[@]}"
"$rsyslogd_bin" -N1 -f "$tmp_dir/omazuredce.yaml" "${module_args[@]}"
