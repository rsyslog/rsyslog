#!/usr/bin/env bash
# Verify YAML support and the modules in the package-profile POC.
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

"$rsyslogd_bin" -N1 -f "$tmp_dir/base.yaml" "${module_args[@]}"

for module in lmnsd_ossl lmnsd_gtls omotel omazuredce; do
	cat > "$tmp_dir/$module.yaml" <<EOF
version: 2
modules:
  - load: $module
rulesets:
  - name: main
    script: |
      action(type="omfile" file="/dev/null")
EOF
	"$rsyslogd_bin" -N1 -f "$tmp_dir/$module.yaml" "${module_args[@]}"
done
