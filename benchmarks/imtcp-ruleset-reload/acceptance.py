#!/usr/bin/env python3
"""Require two identical, counterbalanced Release B sessions before acceptance."""
import argparse
import json
from pathlib import Path


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
    provenance = {(report.get("baseline_revision"), report.get("candidate_revision"),
                   report.get("baseline_source_fingerprint"), report.get("candidate_source_fingerprint"))
                  for report in reports}
    if len(provenance) != 1 or None in next(iter(provenance)):
        raise SystemExit("both sessions must compare identical revisions and source fingerprints")
    if any(report.get("schema_version") != 2 or report.get("stage") != "Release B" for report in reports):
        raise SystemExit("both inputs must be Release B schema-version 2 comparisons")
    if reports[0].get("policy") != reports[1].get("policy") or \
            reports[0].get("policy_fingerprint") != reports[1].get("policy_fingerprint"):
        raise SystemExit("both sessions must use the identical fixed acceptance policy")
    if reports[0].get("workload_set") != reports[1].get("workload_set") or \
            not reports[0].get("full_required_coverage") or not reports[1].get("full_required_coverage"):
        raise SystemExit("both sessions must use the identical complete required workload set")
    for report, manifest in zip(reports, manifests):
        if manifest.get("schema_version") != 1 or manifest.get("session") != report["session"]:
            raise SystemExit("evidence manifest does not identify its comparison session")
        required = manifest.get("required_evidence", {})
        for evidence_name in ("perf_stat", "profile", "disassembly"):
            evidence = required.get(evidence_name, {})
            if evidence.get("applicable") and (evidence.get("status") != "present" or not evidence.get("artifacts")):
                raise SystemExit("required %s evidence is missing" % evidence_name)
    if any(report.get("acceptance", {}).get("result") != "pass" for report in reports):
        raise SystemExit("unsupported, inconclusive, or rejected sessions cannot produce an acceptance report")
    result = {"schema_version": 2, "acceptance_report_version": args.version, "release": "B",
              "baseline_revision": reports[0]["baseline_revision"],
              "candidate_revision": reports[0]["candidate_revision"],
              "baseline_source_fingerprint": reports[0]["baseline_source_fingerprint"],
              "candidate_source_fingerprint": reports[0]["candidate_source_fingerprint"],
              "sessions": ids, "result": "pass", "hot_path_invariants": "passed in every required paired trial",
              "policy": reports[0]["policy"], "policy_fingerprint": reports[0]["policy_fingerprint"],
              "workload_set": reports[0]["workload_set"],
              "evidence_manifests": args.evidence_manifest,
              "raw_artifacts": "local ignored artifacts; retain both raw baseline/candidate JSON files"}
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
