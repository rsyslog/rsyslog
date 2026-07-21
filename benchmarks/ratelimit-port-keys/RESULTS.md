# Port-key optimization results

Measurements were recorded on 2026-07-20 on x86_64 WSL2 with GCC 13.3.0.
Both worktrees used `--enable-debug --enable-testbench --enable-imptcp
--enable-impstats --enable-imdiag`. The host was kept otherwise idle but was
not exclusively reserved, and filesystem/cache state was uncontrolled.

Every session used one unrecorded calibration pair followed by eleven
alternating measured pairs. Ratios are candidate messages/second divided by
baseline messages/second; MAD is the median absolute deviation of those paired
ratios. Each trial validated complete, ordered delivery after its timed sender
interval.

## `%fromhost-port%` shortcut: retained

Baseline `15b12e821c8fb88ca0d7e13d0f35ef8a21e7ba28`, candidate
`91ec8e9e3964b5c422a8bac40444917beae75df0`, 524,288 messages per trial:

| Input | Connections | Session 1 median / MAD | Session 2 median / MAD |
| --- | ---: | ---: | ---: |
| imtcp | 1 | +31.40% / 4.10% | +29.50% / 4.79% |
| imtcp | 32 | +25.80% / 4.34% | +22.47% / 1.41% |
| imtcp | 128 | +41.83% / 30.91% | +3.25% / 30.63% |
| imptcp | 1 | +57.88% / 3.81% | +51.47% / 4.76% |
| imptcp | 32 | +10.44% / 7.38% | +7.99% / 8.33% |
| imptcp | 128 | -1.22% / 0.75% | -2.34% / 1.22% |

The shortcut exceeds its 5% improvement gate in both independent sessions,
and no workload crosses the -3% regression guardrail.

Churn used 32 connections per round, 32 rounds, 131,072 total messages, and
`maxStates=64`:

| Input | Session 1 median / MAD | Session 2 median / MAD |
| --- | ---: | ---: |
| imtcp | +20.31% / 1.96% | +17.61% / 1.33% |
| imptcp | +10.24% / 2.52% | +6.92% / 3.77% |

## Inline tuple buffer: rejected

The candidate removed the common tuple allocation, but two full independent
sessions found no repeatable qualifying gain and several regressions beyond
the -3% guardrail. Examples from session 2 were imptcp IP/port at 32
connections (-6.68%), imtcp IP/port at one connection (-6.72%), and imtcp
IP/port at 32 connections (-16.58%). The inline-buffer commit was therefore
removed from the branch.

Early end-to-end samples are intentionally excluded: their timer included the
testbench's 200 ms file-line polling interval and produced quantized, bimodal
results. The committed harness ends timing at tcpflood sender completion,
keeps polling outside the interval, pins the downstream main queue to one
worker, and still requires exact post-run delivery.
