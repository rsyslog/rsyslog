#!/usr/bin/env python3
"""Require two identical, counterbalanced Release B sessions before acceptance."""
import argparse
import hashlib
import json
from pathlib import Path

from compare import POLICY, policy_fingerprint, required_signature


def artifact_digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--session", action="append", required=True, help="comparison JSON; specify exactly twice")
    parser.add_argument("--evidence-manifest", action="append", required=True,
                        help="required perf/profile/disassembly evidence manifest; specify exactly twice")
    parser.add_argument("--output", required=True)
    parser.add_argument("--version", default="release-b-v1")
    args = parser.parse_args()
    if len(args.session) != 2:
        parser.error("Release B requires exactly two independent sessions")
    if len(args.evidence_manifest) != 2:
        parser.error("Release B requires exactly two evidence manifests")
    reports = [json.loads(Path(name).read_text(encoding="utf-8")) for name in args.session]
    manifests = [json.loads(Path(name).read_text(encoding="utf-8")) for name in args.evidence_manifest]
    ids = [report.get("session") for report in reports]
    if len(set(ids)) != 2 or not all(ids):
        raise SystemExit("sessions must have distinct nonempty identifiers")
    run_provenance = [report.get("run_provenance") for report in reports]
    if len(set(run_provenance)) != 2 or not all(run_provenance):
        raise SystemExit("sessions must carry distinct raw-run provenance")
    provenance = {(report.get("baseline_revision"), report.get("candidate_revision"),
                   report.get("baseline_source_fingerprint"), report.get("candidate_source_fingerprint"))
                  for report in reports}
    if len(provenance) != 1 or None in next(iter(provenance)):
        raise SystemExit("both sessions must compare identical revisions and source fingerprints")
    if any(report.get("schema_version") != 2 or report.get("stage") != "Release B" for report in reports):
        raise SystemExit("both inputs must be Release B schema-version 2 comparisons")
    if any(report.get("policy") != POLICY or report.get("policy_fingerprint") != policy_fingerprint()
           for report in reports):
        raise SystemExit("both sessions must use the canonical fixed acceptance policy")
    expected_workloads = required_signature()
    if [tuple(item) for item in reports[0].get("workload_set", ())] != expected_workloads or \
            [tuple(item) for item in reports[1].get("workload_set", ())] != expected_workloads or \
            not reports[0].get("full_required_coverage") or not reports[1].get("full_required_coverage"):
        raise SystemExit("both sessions must use the identical complete required workload set")
    for manifest_index, (report, manifest) in enumerate(zip(reports, manifests)):
        if manifest.get("schema_version") != 1 or manifest.get("session") != report["session"]:
            raise SystemExit("evidence manifest does not identify its comparison session")
        required = manifest.get("required_evidence", {})
        for evidence_name in ("perf_stat", "profile", "disassembly"):
            if evidence_name not in required:
                raise SystemExit("evidence manifest omits required %s declaration" % evidence_name)
            evidence = required[evidence_name]
            if evidence.get("applicable") and (evidence.get("status") != "present" or not evidence.get("artifacts")):
                raise SystemExit("required %s evidence is missing" % evidence_name)
            for artifact in evidence.get("artifacts", ()):
                artifact_path = (Path(args.evidence_manifest[manifest_index]).parent / artifact).resolve()
                if not artifact_path.is_file():
                    raise SystemExit("required evidence artifact does not exist: %s" % artifact_path)
                declared = evidence.get("sha256", {}).get(artifact)
                if declared != artifact_digest(artifact_path):
                    raise SystemExit("required evidence artifact digest mismatch: %s" % artifact_path)
    if any(report.get("acceptance", {}).get("result") != "pass" for report in reports):
        raise SystemExit("unsupported, inconclusive, or rejected sessions cannot produce an acceptance report")
    for report in reports:
        workloads = report.get("workloads")
        if not isinstance(workloads, list) or len(workloads) != len(required_signature()) or not all(
                row.get("invariants_ok") is True and row.get("status") in ("pass", "validated")
                for row in workloads if isinstance(row, dict)) or not all(isinstance(row, dict) for row in workloads):
            raise SystemExit("session does not prove every required workload invariant")
    result = {"schema_version": 2, "acceptance_report_version": args.version, "release": "B",
              "baseline_revision": reports[0]["baseline_revision"],
              "candidate_revision": reports[0]["candidate_revision"],
              "baseline_source_fingerprint": reports[0]["baseline_source_fingerprint"],
              "candidate_source_fingerprint": reports[0]["candidate_source_fingerprint"],
              "sessions": ids, "result": "pass", "hot_path_invariants": True,
              "policy": reports[0]["policy"], "policy_fingerprint": reports[0]["policy_fingerprint"],
              "workload_set": reports[0]["workload_set"],
              "evidence_manifests": args.evidence_manifest,
              "raw_artifacts": "local ignored artifacts; retain both raw baseline/candidate JSON files"}
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
