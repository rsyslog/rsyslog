#!/usr/bin/env python3

"""Regression tests for the local security-review input builder.

The suite creates isolated Git repositories so each test controls the base,
index, worktree, and untracked state. The oracle is the emitted JSON inventory,
route, and digest, or a deterministic schema-validation failure.
"""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path


BUILDER = Path(__file__).resolve().parents[1] / "build-security-review-input.py"
BOUNDARIES = ["TB-CI", "TB-PIPELINE"]
INVARIANTS = ["SI-CI-TRUST-01", "SI-PARSER-SAFETY-01"]


class SecurityReviewInputTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.repo = Path(self.tempdir.name)
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Security Review Test")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "gc.auto", "0")
        self.git("config", "maintenance.auto", "false")
        self.write("SECURITY.md", "# Security\n")
        self.write("doc/README.md", "internal documentation\n")
        self.write(
            "doc/security/project-threat-model.md",
            "# Model\n\nModel revision: 1\n\n### TB-CI — CI\n\n### TB-PIPELINE — Pipeline\n\n"
            "### SI-CI-TRUST-01 — CI trust\n\n### SI-PARSER-SAFETY-01 — Parser safety\n",
        )
        self.write_map(self.default_components())
        self.write("runtime/existing.c", "int existing(void) { return 0; }\n")
        self.git("add", ".")
        self.git("commit", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def git(self, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", *args], cwd=self.repo, text=True, capture_output=True, check=check
        )

    def write(self, relative: str, content: str | bytes) -> None:
        path = self.repo / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8")

    @staticmethod
    def default_components() -> list[dict[str, object]]:
        return [
            {
                "id": "runtime",
                "paths": ["runtime/**"],
                "boundaries": ["TB-PIPELINE"],
                "invariants": ["SI-PARSER-SAFETY-01"],
                "exposure": "shared-runtime",
                "evidence": ["runtime/existing.c"],
                "test_hints": ["unit"],
                "requires_lead": False,
            },
            {
                "id": "security-review-policy",
                "paths": [
                    "SECURITY.md",
                    "doc/security/project-threat-model.md",
                    "doc/security/threat-model-components.json",
                ],
                "boundaries": ["TB-CI"],
                "invariants": ["SI-CI-TRUST-01"],
                "exposure": "repository-policy",
                "evidence": ["SECURITY.md"],
                "test_hints": [],
                "requires_lead": True,
            },
        ]

    def write_map(self, components: list[dict[str, object]]) -> None:
        self.write(
            "doc/security/threat-model-components.json",
            json.dumps({"schema_version": 1, "model_revision": 1, "components": components}) + "\n",
        )

    def run_builder(
        self, *, check: bool = True, extra_args: list[str] | None = None
    ) -> tuple[subprocess.CompletedProcess[str], dict[str, object] | None]:
        result = subprocess.run(
            [
                "python3",
                str(BUILDER),
                "--repo",
                str(self.repo),
                "--base",
                self.base,
                *(extra_args or []),
                "--output", "-",
            ],
            text=True,
            capture_output=True,
            check=check,
        )
        return result, json.loads(result.stdout) if result.returncode == 0 else None

    def changes(self, package: dict[str, object]) -> dict[str, dict[str, object]]:
        return {item["path"]: item for item in package["changes"]}

    def test_accounts_for_committed_staged_unstaged_and_untracked_changes(self) -> None:
        self.write("runtime/committed.c", "int committed;\n")
        self.git("add", "runtime/committed.c")
        self.git("commit", "-qm", "committed change")
        self.write("runtime/staged.c", "int staged;\n")
        self.git("add", "runtime/staged.c")
        self.write("runtime/existing.c", "int existing(void) { return 1; }\n")
        self.write("runtime/untracked.c", "int untracked;\n")

        _, package = self.run_builder()
        items = self.changes(package)
        self.assertEqual(
            {"runtime/committed.c", "runtime/staged.c", "runtime/existing.c", "runtime/untracked.c"},
            set(items),
        )
        self.assertEqual("M", items["runtime/existing.c"]["status"])
        self.assertTrue(package["worktree"]["staged"])
        self.assertTrue(package["worktree"]["unstaged"])
        self.assertTrue(package["worktree"]["untracked"])

    def test_rename_and_delete_retain_baseline_locations(self) -> None:
        self.write("runtime/delete.c", "int delete_me;\n")
        self.write("runtime/rename.c", "int rename_me;\n")
        self.git("add", ".")
        self.git("commit", "-qm", "add rename inputs")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()
        self.git("mv", "runtime/rename.c", "runtime/renamed.c")
        self.git("rm", "-q", "runtime/delete.c")

        _, package = self.run_builder()
        items = self.changes(package)
        self.assertEqual("R", items["runtime/renamed.c"]["status"])
        self.assertEqual("runtime/rename.c", items["runtime/renamed.c"]["old_path"])
        self.assertIn(":runtime/rename.c", items["runtime/renamed.c"]["baseline_location"])
        self.assertEqual("D", items["runtime/delete.c"]["status"])
        self.assertIn(":runtime/delete.c", items["runtime/delete.c"]["baseline_location"])

    def test_rename_out_of_source_keeps_old_component_and_review_scope(self) -> None:
        self.write("runtime/moved.c", "int moved;\n")
        self.git("add", "runtime/moved.c")
        self.git("commit", "-qm", "add source to move")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()
        self.git("mv", "runtime/moved.c", "doc/moved.md")

        _, package = self.run_builder()
        item = self.changes(package)["doc/moved.md"]
        self.assertTrue(item["reviewable"])
        self.assertEqual(["runtime"], item["components"])
        self.assertEqual(["doc/moved.md"], package["coverage"]["expected_files"])

    def test_renamed_filename_with_tab_keeps_numstat_accounting(self) -> None:
        original = "".join(f"int line_{number};\n" for number in range(10))
        self.write("runtime/rename-me.c", original)
        self.git("add", "runtime/rename-me.c")
        self.git("commit", "-qm", "add rename input")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()
        self.git("mv", "runtime/rename-me.c", "runtime/renamed\tfile.c")
        self.write("runtime/renamed\tfile.c", original + "int another;\n")
        _, package = self.run_builder()
        item = self.changes(package)["runtime/renamed\tfile.c"]
        self.assertEqual("R", item["status"])
        self.assertEqual(1, item["changed_lines"])

    def test_digest_is_stable_and_any_untracked_content_change_invalidates_it(self) -> None:
        self.write("runtime/new.c", "int value = 1;\n")
        first = self.run_builder()[1]["diff_digest"]
        second = self.run_builder()[1]["diff_digest"]
        self.assertEqual(first, second)
        self.write("runtime/new.c", "int value = 2;\n")
        third = self.run_builder()[1]["diff_digest"]
        self.assertNotEqual(first, third)

    def test_docs_only_is_not_applicable_and_root_pattern_does_not_match_nested_name(self) -> None:
        self.write("doc/README.md", "changed prose only\n")
        self.write("doc/AGENTS.md", "nested guide only\n")
        _, package = self.run_builder()
        self.assertEqual("not_applicable", package["route"])
        self.assertEqual([], package["coverage"]["expected_files"])

    def test_known_code_is_quick_and_covered_once(self) -> None:
        self.write("runtime/new.c", "int value;\n")
        _, package = self.run_builder()
        self.assertEqual("quick", package["route"])
        self.assertEqual(["runtime/new.c"], package["coverage"]["expected_files"])
        self.assertEqual(["runtime"], self.changes(package)["runtime/new.c"]["components"])

    def test_executable_without_suffix_and_sample_configuration_are_reviewable(self) -> None:
        self.write("devtools/local-helper", "#!/bin/sh\nexit 0\n")
        (self.repo / "devtools/local-helper").chmod(0o755)
        self.write("packaging/example.conf", "module(load=\"imtcp\")\n")
        self.write("packaging/build-policy.json", "{}\n")
        _, package = self.run_builder()
        items = self.changes(package)
        self.assertTrue(items["devtools/local-helper"]["reviewable"])
        self.assertTrue(items["packaging/example.conf"]["reviewable"])
        self.assertTrue(items["packaging/build-policy.json"]["reviewable"])

    def test_model_and_map_changes_require_lead(self) -> None:
        model = self.repo / "doc/security/project-threat-model.md"
        model.write_text(model.read_text(encoding="utf-8") + "\nAssumption.\n", encoding="utf-8")
        _, package = self.run_builder()
        self.assertEqual("lead_required", package["route"])
        self.assertIn("security-review-policy", " ".join(package["route_reasons"]))

    def test_custom_model_path_change_requires_lead(self) -> None:
        self.write(
            "doc/security/custom-model.md",
            (self.repo / "doc/security/project-threat-model.md").read_text(encoding="utf-8"),
        )
        self.git("add", "doc/security/custom-model.md")
        self.git("commit", "-qm", "add custom model")
        self.write(
            "doc/security/custom-model.md",
            (self.repo / "doc/security/custom-model.md").read_text(encoding="utf-8") + "\nAssumption.\n",
        )
        _, package = self.run_builder(extra_args=["--model", "doc/security/custom-model.md"])
        self.assertEqual("lead_required", package["route"])

    def test_custom_component_map_path_change_requires_lead(self) -> None:
        self.write_map(self.default_components())
        self.git("mv", "doc/security/threat-model-components.json", "doc/security/custom-components.json")
        self.git("commit", "-qm", "add custom component map")
        custom_map = self.repo / "doc/security/custom-components.json"
        custom_map.write_text(custom_map.read_text(encoding="utf-8") + "\n", encoding="utf-8")
        _, package = self.run_builder(extra_args=["--component-map", "doc/security/custom-components.json"])
        self.assertEqual("lead_required", package["route"])

    def test_invalid_model_utf8_is_reported_without_traceback(self) -> None:
        self.write("doc/security/project-threat-model.md", b"\xff\xfe")
        result, _ = self.run_builder(check=False)
        self.assertEqual(2, result.returncode)
        self.assertIn("invalid threat model UTF-8", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_checked_in_test_hints_exist(self) -> None:
        repository = Path(__file__).resolve().parents[2]
        component_map = json.loads(
            (repository / "doc/security/threat-model-components.json").read_text(encoding="utf-8")
        )
        for component in component_map["components"]:
            for hint in component["test_hints"]:
                if hint.startswith("tests/"):
                    self.assertTrue(
                        any(repository.glob(hint)), f"missing test hint for {component['id']}: {hint}"
                    )

    def test_unknown_source_path_is_conservatively_unmapped(self) -> None:
        self.write("plugins/newinput/newinput.c", "int newinput;\n")
        _, package = self.run_builder()
        self.assertEqual("lead_required", package["route"])
        self.assertEqual(["plugins/newinput/newinput.c"], package["coverage"]["unmapped_files"])
        self.assertEqual(["unmapped-code"], self.changes(package)["plugins/newinput/newinput.c"]["components"])

    def test_component_overlap_keeps_one_file_inventory_entry(self) -> None:
        components = self.default_components()
        components.insert(
            1,
            {
                "id": "parser-overlap",
                "paths": ["runtime/new.c"],
                "boundaries": ["TB-PIPELINE"],
                "invariants": ["SI-PARSER-SAFETY-01"],
                "exposure": "specific-parser",
                "evidence": ["runtime/existing.c"],
                "test_hints": [],
                "requires_lead": False,
            },
        )
        self.write_map(components)
        self.git("add", "doc/security/threat-model-components.json")
        self.git("commit", "-qm", "overlap map")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()
        self.write("runtime/new.c", "int value;\n")
        _, package = self.run_builder()
        self.assertEqual(1, len(package["changes"]))
        self.assertEqual(["runtime", "parser-overlap"], package["changes"][0]["components"])

    def test_invalid_map_references_and_duplicate_ids_are_rejected(self) -> None:
        components = self.default_components()
        components[0]["boundaries"] = ["TB-MISSING"]
        self.write_map(components)
        result, _ = self.run_builder(check=False)
        self.assertEqual(2, result.returncode)
        self.assertIn("unknown model id TB-MISSING", result.stderr)

        components = self.default_components()
        components.append(dict(components[0]))
        self.write_map(components)
        result, _ = self.run_builder(check=False)
        self.assertEqual(2, result.returncode)
        self.assertIn("duplicate component id: runtime", result.stderr)

    def test_model_and_component_map_revisions_must_match(self) -> None:
        model = self.repo / "doc/security/project-threat-model.md"
        model.write_text(model.read_text(encoding="utf-8").replace("revision: 1", "revision: 2"), encoding="utf-8")
        result, _ = self.run_builder(check=False)
        self.assertEqual(2, result.returncode)
        self.assertIn("does not match threat model 2", result.stderr)

    def test_invalid_and_missing_evidence_paths_are_rejected(self) -> None:
        components = self.default_components()
        components[0]["evidence"] = ["../outside"]
        self.write_map(components)
        result, _ = self.run_builder(check=False)
        self.assertIn("invalid repository-relative evidence path", result.stderr)

        components[0]["evidence"] = ["runtime/missing.c"]
        self.write_map(components)
        result, _ = self.run_builder(check=False)
        self.assertIn("evidence path does not exist", result.stderr)

    def test_binary_file_is_inventory_item_with_zero_changed_lines(self) -> None:
        self.write("runtime/binary.c", b"source\x00binary\n")
        _, package = self.run_builder()
        item = self.changes(package)["runtime/binary.c"]
        self.assertTrue(item["binary"])
        self.assertEqual(0, item["changed_lines"])

    def test_expanded_route_for_file_and_line_thresholds(self) -> None:
        for number in range(51):
            self.write(f"runtime/generated-{number}.c", "int generated;\n")
        _, package = self.run_builder()
        self.assertEqual("expanded", package["route"])
        self.assertEqual(51, package["summary"]["reviewable_files"])

        self.git("clean", "-fdq")
        self.write("runtime/large.c", "int line;\n" * 2001)
        _, package = self.run_builder()
        self.assertEqual("expanded", package["route"])
        self.assertEqual(2001, package["summary"]["reviewable_changed_lines"])


if __name__ == "__main__":
    unittest.main()
