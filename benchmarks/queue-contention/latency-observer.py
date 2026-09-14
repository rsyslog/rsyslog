#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fixed-rate TCP sender and exact-ID file-observation latency oracle.

Latency is sendall-to-complete-output-line observation in this one process's
monotonic clock domain.  It includes omfile visibility, reader polling, and
scheduler delay; no observation overhead is subtracted.
"""
import argparse
import json
import math
from pathlib import Path
import re
import socket
import statistics
import threading
import time

LINE = re.compile(r'^latency:([0-9]+):([0-9]+)$')


def percentile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    return values[math.ceil(fraction * len(values)) - 1]


def rate_is_valid(offered_rate, achieved_rate, tolerance_percent):
    return abs(achieved_rate - offered_rate) * 100 / offered_rate <= tolerance_percent


class Observer:
    def __init__(self, output, expected, poll_ns):
        self.output, self.expected, self.poll_ns = Path(output), set(expected), poll_ns
        self.seen, self.invalid, self.duplicates = {}, 0, 0
        self.max_poll_gap_ns, self._stop = 0, threading.Event()

    def run(self):
        offset, pending, previous = 0, b'', time.monotonic_ns()
        while not self._stop.is_set():
            now = time.monotonic_ns()
            self.max_poll_gap_ns = max(self.max_poll_gap_ns, now - previous)
            previous = now
            try:
                with self.output.open('rb') as stream:
                    stream.seek(offset)
                    pending += stream.read()
                    offset = stream.tell()
            except FileNotFoundError:
                pass
            lines = pending.split(b'\n')
            pending = lines.pop()
            for raw in lines:
                match = LINE.fullmatch(raw.decode('ascii', errors='replace').rstrip('\r'))
                if match is None:
                    self.invalid += 1
                    continue
                identifier, sent_ns = int(match.group(1)), int(match.group(2))
                if identifier not in self.expected or identifier in self.seen:
                    self.duplicates += identifier in self.seen
                    self.invalid += identifier not in self.expected
                    continue
                self.seen[identifier] = (sent_ns, time.monotonic_ns())
            time.sleep(self.poll_ns / 1e9)

    def stop(self):
        self._stop.set()


def run(args):
    if min(args.messages, args.connections, args.rate, args.poll_us) <= 0:
        raise ValueError('messages, connections, rate, and poll-us must be positive')
    if args.messages < 2:
        raise ValueError('messages must be at least two for an achieved-rate validity check')
    expected = range(args.id_start, args.id_start + args.messages)
    observer = Observer(args.output, expected, args.poll_us * 1000)
    reader = threading.Thread(target=observer.run, name='latency-reader')
    sent, lateness, errors, lock = {}, [], [], threading.Lock()
    start_ns = time.monotonic_ns() + args.warmup_ms * 1000000
    period_ns = 1_000_000_000 / args.rate

    def sender(connection):
        try:
            with socket.create_connection((args.host, args.port), timeout=args.connect_timeout) as stream:
                for ordinal in range(connection, args.messages, args.connections):
                    deadline = start_ns + int(ordinal * period_ns)
                    delay = deadline - time.monotonic_ns()
                    if delay > 0:
                        time.sleep(delay / 1e9)
                    sent_ns = time.monotonic_ns()  # immediately before this exact sendall
                    identifier = args.id_start + ordinal
                    stream.sendall(('latency:%d:%d\n' % (identifier, sent_ns)).encode('ascii'))
                    with lock:
                        sent[identifier] = sent_ns
                        lateness.append(max(0, sent_ns - deadline))
        except OSError as error:
            with lock:
                errors.append(str(error))

    reader.start()
    workers = [threading.Thread(target=sender, args=(connection,)) for connection in range(args.connections)]
    for worker in workers:
        worker.start()
    for worker in workers:
        worker.join()
    completion_deadline = time.monotonic_ns() + args.completion_timeout * 1_000_000_000
    while len(observer.seen) < args.messages and time.monotonic_ns() < completion_deadline:
        time.sleep(args.poll_us / 1e6)
    observer.stop()
    reader.join()
    missing = sorted(set(expected) - set(observer.seen))
    timestamp_mismatch = sum(sent.get(identifier) != stamp for identifier, (stamp, _) in observer.seen.items())
    observations = [observed - stamp for stamp, observed in observer.seen.values() if observed >= stamp]
    ordered_sends = sorted(sent.values())
    offered_duration_ns = ordered_sends[-1] - ordered_sends[0] if len(ordered_sends) > 1 else 0
    achieved_rate = ((args.messages - 1) * 1e9 / offered_duration_ns) if offered_duration_ns else 0.0
    rate_drift_percent = abs(achieved_rate - args.rate) * 100 / args.rate
    valid = (not errors and not missing and not observer.duplicates and not observer.invalid and not timestamp_mismatch
             and len(sent) == args.messages and rate_is_valid(args.rate, achieved_rate, args.max_rate_drift_percent)
             and max(lateness, default=0) <= args.max_lateness_ms * 1_000_000
             and observer.max_poll_gap_ns <= args.max_poll_gap_ms * 1_000_000)
    return {
        'schema_version': 1, 'status': 'completed' if valid else 'invalid', 'latency_definition':
        'monotonic sendall-to-complete-file-line observation; includes output flush, polling, and scheduler delay',
        'messages': args.messages, 'connections': args.connections, 'offered_rate_per_second': args.rate,
        'achieved_rate_per_second': achieved_rate, 'offered_duration_ns': offered_duration_ns,
        'rate_drift_percent': rate_drift_percent,
        'scheduler_lateness_ns': {'p99': percentile(lateness, .99), 'max': max(lateness, default=0),
                                   'limit': args.max_lateness_ms * 1_000_000},
        'reader': {'poll_interval_ns': args.poll_us * 1000, 'max_poll_gap_ns': observer.max_poll_gap_ns,
                   'max_poll_gap_limit_ns': args.max_poll_gap_ms * 1_000_000,
                   'observation_bound': 'complete-line polling/scheduling delay is included, not subtracted'},
        'latency_ns': {'p50': percentile(observations, .50), 'p95': percentile(observations, .95),
                       'p99': percentile(observations, .99), 'max': max(observations, default=None)},
        'oracle': {'received': len(observer.seen), 'missing': missing, 'duplicates': observer.duplicates,
                   'invalid_output': observer.invalid, 'timestamp_mismatch': timestamp_mismatch, 'send_errors': errors},
        'validity_thresholds': {'max_rate_drift_percent': args.max_rate_drift_percent,
                                'max_scheduler_lateness_ms': args.max_lateness_ms,
                                'max_reader_poll_gap_ms': args.max_poll_gap_ms}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', required=True, type=int)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--result', required=True, type=Path)
    parser.add_argument('--messages', required=True, type=int)
    parser.add_argument('--connections', required=True, type=int)
    parser.add_argument('--rate', required=True, type=float)
    parser.add_argument('--id-start', type=int, default=0)
    parser.add_argument('--poll-us', type=int, default=100)
    parser.add_argument('--warmup-ms', type=int, default=100)
    parser.add_argument('--completion-timeout', type=int, default=30)
    parser.add_argument('--connect-timeout', type=int, default=10)
    parser.add_argument('--max-rate-drift-percent', type=float, default=2.0)
    parser.add_argument('--max-lateness-ms', type=float, default=10.0)
    parser.add_argument('--max-poll-gap-ms', type=float, default=5.0)
    args = parser.parse_args()
    result = run(args)
    args.result.write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'completed':
        raise SystemExit('latency observation invalid: ' + json.dumps(result['oracle']))


if __name__ == '__main__':
    main()
