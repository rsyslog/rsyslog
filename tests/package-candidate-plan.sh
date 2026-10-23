#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# The candidate planner must preserve all seven existing build routes and both
# canonical architectures while rejecting invalid selections and catalogs.
# A failed plan must emit no matrix, so a workflow cannot dispatch partial work.
# Build selection maps canonical architectures to the reviewed workflow's
# native rows and retains its builder parameters; a missing row must fail.
set -eu

repo_root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
command -v python3 >/dev/null 2>&1 || exit 77
python3 - "$repo_root" <<'PY'
import copy
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

if not __debug__:
    raise SystemExit("This test requires Python assertions; do not use -O")

root = Path(sys.argv[1])
catalog = json.loads((root / ".github/packaging-targets.json").read_text())
expected = {
    "debian-13": "debian_daily_stable.yml",
    "ubuntu-26.04": "ubuntu_daily_stable.yml",
    "el-10": "el10_daily_stable.yml",
    "fedora-44": "fedora44_daily_stable.yml",
    "opensuse-16.0": "opensuse_leap160_daily_stable.yml",
    "alpine-3.24": "alpine324_daily_stable.yml",
    "amazonlinux-2023": "amazonlinux2023_daily_stable.yml",
}


with tempfile.TemporaryDirectory(prefix="rsyslog-candidate-plan-") as temporary:
    # Only selector structure is exercised here. These empty files satisfy
    # path validation; they do not stand in for real builder compatibility.
    fixture_root = Path(temporary) / "repo"
    planner = fixture_root / "devtools/release/package-candidate-plan.py"
    planner.parent.mkdir(parents=True)
    shutil.copyfile(root / "devtools/release/package-candidate-plan.py", planner)
    default_catalog = fixture_root / ".github/packaging-targets.json"
    default_catalog.parent.mkdir(parents=True)
    default_catalog.write_text(json.dumps(catalog))
    for row in catalog["targets"]:
        route = row.get("existing_archive")
        if route is None:
            continue
        for kind in ("workflow", "policy", "builder"):
            path = fixture_root / route[kind]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
    fixture = Path(temporary) / "targets.json"

    def run(*args, source=None):
        command = [sys.executable, str(planner)]
        if source is not None:
            command += ["--catalog", str(source)]
        return subprocess.run(command + list(args), capture_output=True, text=True)

    def succeeds(*args, source=None):
        result = run(*args, source=source)
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout)

    def fails(*args, source=None):
        result = run(*args, source=source)
        assert result.returncode != 0, (args, result.stdout)
        assert not result.stdout, (args, result.stdout)
        assert result.stderr, args

    all_routes = succeeds("--targets", "existing")["include"]
    assert len(all_routes) == len(expected), all_routes
    assert {route["target"]: route["workflow"] for route in all_routes} == expected
    assert all(route["architectures"] == ["x86_64", "aarch64"] for route in all_routes)

    selected = succeeds("--targets", "el-10,debian-13")["include"]
    assert [route["target"] for route in selected] == ["el-10", "debian-13"]
    assert [route["workflow"] for route in selected] == [expected["el-10"], expected["debian-13"]]

    listing = succeeds("--list")["targets"]
    assert len(listing) == len(catalog["targets"]), listing
    assert {row["target"] for row in listing if row["route"] == "available"} == set(expected)

    fails("--targets", "unknown-target")
    fails("--targets", "debian-12")  # A cataloged target without a builder.
    fails("--targets", "el-10,el-10")

    def invalid(changed, *arguments):
        fixture.write_text(json.dumps(changed))
        fails(*(arguments or ("--targets", "existing")), source=fixture)

    invalid({"schema_version": 1, "targets": "not a list"})
    invalid({"schema_version": 1, "targets": []}, "--targets", "existing")
    bad_format = copy.deepcopy(catalog)
    bad_format["targets"][0]["package_format"] = ["deb"]
    invalid(bad_format)
    duplicate = copy.deepcopy(catalog)
    duplicate["targets"].append(copy.deepcopy(duplicate["targets"][0]))
    invalid(duplicate)

    escaped = copy.deepcopy(catalog)
    route = next(row["existing_archive"] for row in escaped["targets"]
                 if row["id"] == "el-10")
    route["workflow"] = ".github/workflows/../../outside.yml"
    invalid(escaped, "--targets", "el-10")

    missing_policy = copy.deepcopy(catalog)
    route = next(row["existing_archive"] for row in missing_policy["targets"]
                 if row["id"] == "el-10")
    route["policy"] = ".github/nonexistent-policy.yml"
    invalid(missing_policy, "--targets", "el-10")

    def build(target, workflow, rows, source=None):
        return succeeds("--build-target", target, "--workflow", workflow,
                        "--native-matrix", json.dumps({"include": rows}), source=source)

    def build_fails(target, workflow, native, source=None):
        fails("--build-target", target, "--workflow", workflow,
              "--native-matrix", native, source=source)

    # Debian's catalog aliases select workflow-native amd64/arm64 in catalog
    # order, even when the reviewed workflow matrix lists ARM first.
    deb_rows = [{"arch": "arm64", "runner": "ubuntu-24.04-arm"},
                {"arch": "amd64", "runner": "ubuntu-24.04"}]
    deb = build("debian-13", expected["debian-13"], deb_rows)
    assert [row["arch"] for row in deb["matrix"]["include"]] == ["amd64", "arm64"]
    assert deb["policy"] == ".github/debian-daily-stable-13-policy.yml"

    # RPM uses canonical architecture names and must preserve per-arch mock
    # configuration from the reviewed workflow matrix.
    rpm_rows = [{"arch": "aarch64", "runner": "ubuntu-24.04-arm", "mock_config": "arm-config"},
                {"arch": "x86_64", "runner": "ubuntu-24.04", "mock_config": "x86-config"}]
    rpm = build("el-10", expected["el-10"], rpm_rows)
    assert rpm["matrix"]["include"] == list(reversed(rpm_rows))

    # A deliberate catalog architecture subset dispatches one native row.
    subset = copy.deepcopy(catalog)
    next(row for row in subset["targets"] if row["id"] == "el-10")["architectures"] = ["aarch64"]
    fixture.write_text(json.dumps(subset))
    assert build("el-10", expected["el-10"], rpm_rows, fixture)["matrix"]["include"] == [rpm_rows[0]]

    build_fails("el-10", expected["debian-13"], json.dumps({"include": rpm_rows}))
    build_fails("debian-12", expected["debian-13"], json.dumps({"include": deb_rows}))
    build_fails("debian-13", expected["debian-13"],
                json.dumps({"include": [deb_rows[0]]}))  # Missing amd64.
    build_fails("el-10", expected["el-10"],
                json.dumps({"include": rpm_rows + [rpm_rows[0]]}))
    for malformed in ('{"include":{}}', '{',
                      json.dumps({"include": rpm_rows, "exclude": [rpm_rows[0]]}), json.dumps({"include": [{"arch": 42, "runner": "ok"}]}),
                      json.dumps({"include": [{"arch": "x86_64", "runner": ["ubuntu-24.04"]}]})):
        build_fails("el-10", expected["el-10"], malformed)

    # A policy output is written to GITHUB_OUTPUT by the adapter; reject
    # paths that could inject lines or point outside the policy directory.
    for unsafe in (".github/policy.yml\ninjected=1", ".github/../policy.yml",
                   "devtools/release/policy.yml"):
        unsafe_file = fixture_root / unsafe
        unsafe_file.parent.mkdir(parents=True, exist_ok=True)
        unsafe_file.touch()  # Existence must not mask unsafe path rejection.
        bad = copy.deepcopy(catalog)
        next(row for row in bad["targets"] if row["id"] == "el-10")["existing_archive"]["policy"] = unsafe
        fixture.write_text(json.dumps(bad))
        build_fails("el-10", expected["el-10"], json.dumps({"include": rpm_rows}), fixture)

print("package candidate planner: PASS")
PY
