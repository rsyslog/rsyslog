#!/usr/bin/env python3
"""Deterministic self-tests for benchmark workload and comparison plumbing."""

import tempfile
from pathlib import Path
import subprocess

import compare
import runner


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    # The workload oracle protects the practical 1024-message batching model,
    # all representative payloads, and a forced-spill DA message count.
    matrix = runner.workloads("all", ("segmented", "da"), runner.PAYLOADS,
                              runner.TARGET_BYTES, 1024)
    check(len(matrix) == 18, "timed matrix must cover both modes, three payloads, and three valid scenarios")
    check(not any(item["scenario"] == "restart-replay" for item in matrix),
          "restart must remain functional coverage instead of a confounded timing workload")
    check(all(item["batch_size"] == 1024 for item in matrix), "batch size must propagate unchanged")
    da_32k = next(item for item in matrix if item["scenario"] == "throughput" and
                  item["mode"] == "da" and item["payload_bytes"] == 32768)
    check(da_32k["messages"] == 12288, "DA 32-KiB workload must force twelve batches")
    rotation = next(item for item in matrix if item["scenario"] == "rotation" and
                    item["mode"] == "segmented" and item["payload_bytes"] == 32768)
    check(rotation["segment_bytes"] == 262144, "rotation workload must use the stress segment size")

    # strace output has an optional error-count column. Both shapes must map to
    # structured counts without retaining machine-specific text.
    summary = runner.parse_strace_summary(
        " 12.00 0.120000 12 10 openat\n 8.00 0.080000 8 9 2 read\n100.00 0.200000 10 19 2 total\n")
    check(summary["openat"] == {"seconds": 0.12, "calls": 10, "errors": 0},
          "syscall row without errors parsed incorrectly")
    check(summary["read"] == {"seconds": 0.08, "calls": 9, "errors": 2},
          "syscall row with errors parsed incorrectly")

    record = {"scenario": "throughput", "mode": "da", "payload_bytes": 32768,
              "batch_size": 1024, "segment_bytes": 1048576, "trial": 3}
    check(compare.key(record) == ("throughput", "da", 32768, 1024, 1048576, 3),
          "paired comparison key omitted a workload dimension")
    check(abs(compare.median_absolute_deviation([1.0, 1.1, 1.2]) - 0.1) < 1e-12,
          "median absolute deviation calculation changed")

    # A zero-valued preceding pair is excluded without shifting the trial ID
    # associated with a later ratio or outlier.
    metric_pairs = [
        ({"trial": 4, "spill_ns": 0}, {"trial": 4, "spill_ns": 1}),
        ({"trial": 7, "spill_ns": 20}, {"trial": 7, "spill_ns": 10}),
    ]
    check(compare.paired_metric_ratios(metric_pairs, "spill_ns") == [(7, 2.0)],
          "filtered metric ratio lost its original trial ID")

    matching = (
        {"metadata": {"session": "session-a", "revision": "base", "source_fingerprint": "base-fp",
                      "pair_revision": "candidate", "pair_source_fingerprint": "candidate-fp"}},
        {"metadata": {"session": "session-a", "revision": "candidate",
                      "source_fingerprint": "candidate-fp", "pair_revision": "base",
                      "pair_source_fingerprint": "base-fp"}},
    )
    compare.validate_paired_metadata(*matching)
    try:
        compare.validate_paired_metadata(matching[0], {"metadata": {"session": "session-b"}})
    except SystemExit:
        pass
    else:
        raise AssertionError("comparison accepted records from different sessions")
    mismatched_pair = {"metadata": dict(matching[1]["metadata"], pair_revision="unrelated")}
    try:
        compare.validate_paired_metadata(matching[0], mismatched_pair)
    except SystemExit:
        pass
    else:
        raise AssertionError("comparison accepted unrelated revisions with a reused session")

    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "same.json"
        try:
            runner.validate_pair_outputs(output, output)
        except SystemExit:
            pass
        else:
            raise AssertionError("paired run accepted identical output paths")

        repository = Path(directory) / "repository"
        repository.mkdir()
        subprocess.run(["git", "init", "-q", str(repository)], check=True)
        tracked = repository / "tracked"
        tracked.write_text("original\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(repository), "add", "tracked"], check=True)
        subprocess.run(["git", "-C", str(repository), "-c", "user.name=benchmark-selftest",
                        "-c", "user.email=benchmark-selftest@example.invalid", "-c", "commit.gpgsign=false",
                        "commit", "-qm", "fixture"],
                       check=True)
        clean_fingerprint = runner.source_fingerprint(repository)
        tracked.unlink()
        check(runner.source_fingerprint(repository) != clean_fingerprint,
              "source fingerprint ignored a deleted tracked file")
    print("segmented disk queue benchmark self-tests passed")


if __name__ == "__main__":
    main()
