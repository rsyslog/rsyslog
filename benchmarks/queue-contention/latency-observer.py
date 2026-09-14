#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fixed-rate TCP sender and exact-ID file-observation latency oracle.

Latency is pre-framing-payload-timestamp-to-complete-output-line observation in
this one process's monotonic clock domain. It includes framing, sendall, omfile
visibility, and reader scheduling; no observation overhead is subtracted.
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

LINE = re.compile(r'^latency:([0-9]+):([0-9]+)\|(x*)$')


def percentile(values, fraction):
    if not values:
        return None
    values = sorted(values)
    return values[math.ceil(fraction * len(values)) - 1]


def rate_is_valid(offered_rate, achieved_rate, tolerance_percent):
    return abs(achieved_rate - offered_rate) * 100 / offered_rate <= tolerance_percent


class Observer:
    def __init__(self, output, expected, payload, poll_ns):
        self.output, self.expected, self.payload, self.poll_ns = Path(output), set(expected), payload, poll_ns
        self.seen, self.invalid, self.duplicates = {}, 0, 0
        self.max_poll_gap_ns, self._stop = 0, threading.Event()

    def run(self):
        offset, pending, previous_complete = 0, b'', time.monotonic_ns()
        while not self._stop.is_set():
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
                parsed = parse_line(raw, self.payload)
                if parsed is None:
                    self.invalid += 1
                    continue
                identifier, sent_ns = parsed
                if identifier not in self.expected or identifier in self.seen:
                    self.duplicates += identifier in self.seen
                    self.invalid += identifier not in self.expected
                    continue
                self.seen[identifier] = (sent_ns, time.monotonic_ns())
            # Include read and parsing time, including the final pass, in the
            # actual reader cadence. This bounds user-space re-observation
            # intervals; it is not a bound on kernel or daemon latency.
            complete = time.monotonic_ns()
            self.max_poll_gap_ns = max(self.max_poll_gap_ns, complete - previous_complete)
            previous_complete = complete
            time.sleep(self.poll_ns / 1e9)

    def stop(self):
        self._stop.set()


def parse_line(raw, payload):
    """Accept only an exact benchmark payload, including all padding bytes."""
    if raw.endswith(b'\r'):
        raw = raw[:-1]
    if len(raw) != payload:
        return None
    match = LINE.fullmatch(raw.decode('ascii', errors='replace'))
    return None if match is None else (int(match.group(1)), int(match.group(2)))


def load_expected(path, expected):
    """Load the sender's exact ID/timestamp manifest written before shutdown."""
    values = {}
    for raw in Path(path).read_text().splitlines():
        identifier, separator, timestamp = raw.partition(' ')
        if not separator or not identifier.isdecimal() or not timestamp.isdecimal():
            raise ValueError('invalid expected timestamp manifest')
        identifier = int(identifier)
        if identifier in values:
            raise ValueError('duplicate ID in expected timestamp manifest')
        values[identifier] = int(timestamp)
    if set(values) != set(expected):
        raise ValueError('expected timestamp manifest does not contain exactly the expected IDs')
    return values


def final_oracle(output, expected, payload, expected_timestamps):
    """Parse the complete post-shutdown sink, including a trailing fragment."""
    data = Path(output).read_bytes() if Path(output).exists() else b''
    lines = data.split(b'\n')
    trailing = lines.pop()
    seen, duplicates, invalid, timestamp_mismatch = set(), 0, int(bool(trailing)), 0
    for raw in lines:
        parsed = parse_line(raw, payload)
        if parsed is None:
            invalid += 1
            continue
        identifier, timestamp = parsed
        if identifier in seen:
            duplicates += 1
        elif identifier not in expected:
            invalid += 1
        else:
            seen.add(identifier)
            timestamp_mismatch += timestamp != expected_timestamps[identifier]
    return {'received': len(seen), 'missing': sorted(expected - seen), 'duplicates': duplicates,
            'invalid_output': invalid, 'timestamp_mismatch': timestamp_mismatch, 'trailing_bytes': len(trailing)}


def run(args):
    if min(args.messages, args.connections, args.rate, args.poll_us) <= 0:
        raise ValueError('messages, connections, rate, and poll-us must be positive')
    if args.messages < 2:
        raise ValueError('messages must be at least two for an achieved-rate validity check')
    expected = range(args.id_start, args.id_start + args.messages)
    minimum_payload = len('latency:') + len(str(args.id_start + args.messages - 1)) + 1 + 20 + 1
    if args.payload < minimum_payload:
        raise ValueError('payload is too short for an exact latency ID/timestamp record')
    observer = Observer(args.output, expected, args.payload, args.poll_us * 1000)
    reader = threading.Thread(target=observer.run, name='latency-reader')
    sent, dispatches, lateness, preparation, errors, lock = {}, {}, [], [], [], threading.Lock()
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
                    sent_ns = time.monotonic_ns()
                    identifier = args.id_start + ordinal
                    record = 'latency:%d:%d' % (identifier, sent_ns)
                    body = record + '|' + 'x' * (args.payload - len(record) - 1)
                    # RFC5424 requires hostname, app-name, procid, msgid,
                    # and structured-data before MSG.  The five NILVALUE
                    # fields keep body in $msg for the stock parser.
                    message = '<13>1 2026-01-01T00:00:00Z - - - - - %s' % body
                    frame = ('%d %s' % (len(message), message)).encode('ascii')
                    dispatch_ns = time.monotonic_ns()  # immediately before the sendall syscall
                    stream.sendall(frame)
                    with lock:
                        sent[identifier] = sent_ns
                        dispatches[identifier] = dispatch_ns
                        preparation.append(dispatch_ns - sent_ns)
                        lateness.append(max(0, dispatch_ns - deadline))
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
    with args.expected.open('w') as expected_file:
        for identifier in sorted(sent):
            expected_file.write('%d %d\n' % (identifier, sent[identifier]))
    missing = sorted(set(expected) - set(observer.seen))
    timestamp_mismatch = sum(sent.get(identifier) != stamp for identifier, (stamp, _) in observer.seen.items())
    observations = [observed - stamp for stamp, observed in observer.seen.values() if observed >= stamp]
    ordered_sends = sorted(dispatches.values())
    offered_duration_ns = ordered_sends[-1] - ordered_sends[0] if len(ordered_sends) > 1 else 0
    achieved_rate = ((args.messages - 1) * 1e9 / offered_duration_ns) if offered_duration_ns else 0.0
    rate_drift_percent = abs(achieved_rate - args.rate) * 100 / args.rate
    valid = (not errors and not missing and not observer.duplicates and not observer.invalid and not timestamp_mismatch
             and len(sent) == args.messages and rate_is_valid(args.rate, achieved_rate, args.max_rate_drift_percent)
             and max(lateness, default=0) <= args.max_lateness_us * 1000
             and max(preparation, default=0) <= args.max_lateness_us * 1000
             and observer.max_poll_gap_ns <= args.max_poll_gap_us * 1000)
    return {
        'schema_version': 1, 'status': 'completed' if valid else 'invalid', 'latency_definition':
        'monotonic pre-send-payload-timestamp-to-complete-file-line observation; '
        'includes framing, sendall, output flush, and reader scheduling',
        'messages': args.messages, 'connections': args.connections, 'offered_rate_per_second': args.rate,
        'achieved_rate_per_second': achieved_rate, 'offered_duration_ns': offered_duration_ns,
        'rate_drift_percent': rate_drift_percent,
        'scheduler_lateness_ns': {'p99': percentile(lateness, .99), 'max': max(lateness, default=0),
                                   'limit': args.max_lateness_us * 1000},
        'pre_send_preparation_ns': {'p99': percentile(preparation, .99), 'max': max(preparation, default=0),
                                    'limit': args.max_lateness_us * 1000,
                                    'definition': 'payload timestamp through immediately-pre-sendall '
                                    'dispatch timestamp'},
        'reader': {'poll_interval_ns': args.poll_us * 1000, 'max_poll_gap_ns': observer.max_poll_gap_ns,
                   'max_poll_gap_limit_ns': args.max_poll_gap_us * 1000,
                   'observation_bound': 'maximum completed reader iteration interval, including sleep, '
                   'file read, and parsing'},
        'latency_ns': {'p50': percentile(observations, .50), 'p95': percentile(observations, .95),
                       'p99': percentile(observations, .99), 'max': max(observations, default=None)},
        'oracle': {'received': len(observer.seen), 'missing': missing, 'duplicates': observer.duplicates,
                   'invalid_output': observer.invalid, 'timestamp_mismatch': timestamp_mismatch, 'send_errors': errors},
        'validity_thresholds': {'max_rate_drift_percent': args.max_rate_drift_percent,
                                'max_scheduler_lateness_us': args.max_lateness_us,
                                'max_pre_send_preparation_us': args.max_lateness_us,
                                'max_reader_iteration_interval_us': args.max_poll_gap_us},
        'measurement_limitations': [
            'The reader interval bounds only user-space re-observation cadence, not daemon, kernel, '
            'or filesystem latency.',
            'The dispatch timestamp precedes sendall; time scheduled inside or after sendall is included '
            'in latency but not separately observable.'
        ]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host')
    parser.add_argument('--port', type=int)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--result', required=True, type=Path)
    parser.add_argument('--expected', required=True, type=Path,
                        help='sender ID/timestamp manifest used by final exact-output validation')
    parser.add_argument('--messages', required=True, type=int)
    parser.add_argument('--connections', type=int)
    parser.add_argument('--rate', type=float)
    parser.add_argument('--payload', type=int)
    parser.add_argument('--id-start', type=int, default=0)
    parser.add_argument('--poll-us', type=int, default=100)
    parser.add_argument('--warmup-ms', type=int, default=100)
    parser.add_argument('--completion-timeout', type=int, default=30)
    parser.add_argument('--connect-timeout', type=int, default=10)
    parser.add_argument('--max-rate-drift-percent', type=float, default=2.0)
    parser.add_argument('--max-lateness-us', type=float, default=400.0)
    parser.add_argument('--max-poll-gap-us', type=float, default=400.0)
    parser.add_argument('--finalize', action='store_true', help='validate the complete sink after daemon shutdown')
    parser.add_argument('--allow-invalid', action='store_true',
                        help='write a provisional invalid result for finalization')
    args = parser.parse_args()
    if args.finalize:
        if args.payload is None:
            parser.error('--payload is required with --finalize')
        result = json.loads(args.result.read_text())
        expected = set(range(args.id_start, args.id_start + args.messages))
        timestamps = load_expected(args.expected, expected)
        final = final_oracle(args.output, expected, args.payload, timestamps)
        result['final_oracle'] = final
        if final['missing'] or final['duplicates'] or final['invalid_output'] or final['timestamp_mismatch']:
            result['status'] = 'invalid'
        args.result.write_text(json.dumps(result, indent=2) + '\n')
        if result['status'] != 'completed':
            raise SystemExit('post-shutdown latency oracle invalid: ' + json.dumps(final))
        return
    if args.host is None or args.port is None:
        parser.error('--host and --port are required unless --finalize is used')
    if args.connections is None or args.rate is None or args.payload is None:
        parser.error('--connections, --rate, and --payload are required unless --finalize is used')
    result = run(args)
    args.result.write_text(json.dumps(result, indent=2) + '\n')
    if result['status'] != 'completed' and not args.allow_invalid:
        raise SystemExit('latency observation invalid: ' + json.dumps(result['oracle']))


if __name__ == '__main__':
    main()
