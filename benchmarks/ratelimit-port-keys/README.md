# Port-key ratelimit benchmark

This benchmark measures input per-source ratelimiting with `%fromhost-port%`,
`%fromhost-ip%:%fromhost-port%`, and `%fromhost%:%fromhost-port%`. Every trial
starts and stops rsyslog, uses deterministic per-connection tcpflood workers,
pins the downstream main queue to one worker, and validates exact delivery
after recording sender completion. Pinning avoids downstream bimodality when
the default queue heuristic starts an additional output worker during only
some trials. The timed interval ends when tcpflood has sent every message;
file polling and validation remain outside it so their 200 ms sampling period
cannot quantize the throughput result.

Build both revisions, then run an alternating paired session:

```sh
benchmarks/ratelimit-port-keys/run.sh \
  --build-dir /path/to/baseline --label baseline \
  --output benchmarks/ratelimit-port-keys/artifacts/baseline.json \
  --pair-build-dir /path/to/candidate --pair-label candidate \
  --pair-output benchmarks/ratelimit-port-keys/artifacts/candidate.json \
  --comparison-output benchmarks/ratelimit-port-keys/artifacts/comparison.json
```

Defaults cover both TCP inputs, all three keys, and 1, 32, and 128
connections. Use `--churn-rounds` to split the message load across repeated
connection lifecycles. Run one calibration plus eleven measured pairs and
repeat the complete session independently before accepting a candidate. The
comparison report records candidate/baseline paired throughput ratios, their
median, and median absolute deviation for every workload. The per-revision
reports include the exact revision, compiler, configure arguments, workload,
and host metadata.
