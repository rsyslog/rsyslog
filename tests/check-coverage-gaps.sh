#!/bin/bash
# This test exercises the local coverage-gap checker without requiring a built
# rsyslog tree.  The oracle is the checker report: changed production lines are
# classified from local lcov data, .codecov.yml ignores, source text, and an
# optional Codecov full-report fixture.

set -eu

srcdir="${srcdir:=.}"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/coverage-gap-checker.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT
mkdir -p "$tmpdir/runtime" "$tmpdir/plugins/imgssapi" "$tmpdir/tests"

cat > "$tmpdir/runtime/foo.c" <<'EOF'
int covered(void) {
	return 1;
}

int uncovered(void) {
	return 2;
}

int cleanup_only(void) {
	free(0);
	return 3;
}
EOF

cat > "$tmpdir/plugins/imgssapi/ignored.c" <<'EOF'
int ignored(void) {
	return 1;
}
EOF

cat > "$tmpdir/.codecov.yml" <<'EOF'
ignore:
  - "plugins/imgssapi/**"
EOF

cat > "$tmpdir/coverage.info" <<EOF
TN:
SF:$tmpdir/runtime/foo.c
FN:1,covered
FNDA:1,covered
FN:5,uncovered
FNDA:0,uncovered
FN:9,cleanup_only
FNDA:0,cleanup_only
DA:1,1
DA:2,1
DA:5,0
DA:6,0
DA:9,0
DA:10,0
end_of_record
EOF

cat > "$tmpdir/diff.patch" <<'EOF'
diff --git a/runtime/foo.c b/runtime/foo.c
index 0000000..1111111 100644
--- a/runtime/foo.c
+++ b/runtime/foo.c
@@ -2,0 +3,8 @@ int covered(void) {
+int uncovered(void) {
+	return 2;
+}
+
+int cleanup_only(void) {
+	free(0);
+	return 3;
+}
diff --git a/plugins/imgssapi/ignored.c b/plugins/imgssapi/ignored.c
index 0000000..1111111 100644
--- a/plugins/imgssapi/ignored.c
+++ b/plugins/imgssapi/ignored.c
@@ -1,0 +2 @@ int ignored(void) {
+	return 1;
EOF

cat > "$tmpdir/codecov.json" <<'EOF'
{
  "files": [
    {
      "name": "runtime/foo.c",
      "line_coverage": [
        [3, 0],
        [4, 0],
        [5, 1],
        [6, 0],
        [7, 0],
        [8, 0],
        [9, 0],
        [10, 0]
      ]
    }
  ]
}
EOF

python3 "$srcdir/../devtools/check-coverage-gaps.py" \
	--repo-root "$tmpdir" \
	--coverage "$tmpdir/coverage.info" \
	--diff-file "$tmpdir/diff.patch" \
	--codecov-report "$tmpdir/codecov.json" \
	--format json > "$tmpdir/report.json"

python3 - "$tmpdir/report.json" <<'PY'
import json
import sys

report = json.load(open(sys.argv[1], encoding="utf-8"))
classes = {(item["classification"], item["start"], item["end"]) for item in report}
assert ("important-gap", 3, 4) in classes, report
assert ("covered-by-ci-only", 5, 5) in classes, report
assert ("low-value-uncovered", 9, 10) in classes, report
assert any(item["classification"] == "ignored" for item in report), report
PY
