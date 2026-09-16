#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Alternate fixed-offered-rate, exact-ID latency observations in two builds."""
import argparse
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
for side in ('before', 'after'):
    p.add_argument('--' + side, required=True, type=Path)
    p.add_argument('--' + side + '-queue-size', type=int, default=32768)
    p.add_argument('--' + side + '-consumer-workers', type=int, default=4)
    p.add_argument('--' + side + '-scope', choices=('global', 'local'), default='global')
p.add_argument('--output', required=True, type=Path)
p.add_argument('--pairs', type=int, default=11)
p.add_argument('--messages', type=int, default=100000)
p.add_argument('--connections', type=int, default=16)
p.add_argument('--input-workers', type=int, default=8)
p.add_argument('--offered-rate', type=float, default=10000)
p.add_argument('--payload', type=int, default=512)
p.add_argument('--frontend-capacity', type=int, default=10000)
p.add_argument('--frontend-max', type=int, default=8)
p.add_argument('--dequeue-batch-size', type=int, default=1024)
p.add_argument('--worker-minimum', type=int, default=1024)
p.add_argument('--flush-policy', choices=('sync',), default='sync')
p.add_argument('--poll-us', type=int, default=100)
p.add_argument('--image', default='rsyslog/rsyslog_dev_base_ubuntu:26.04')
p.add_argument('--trial-timeout', type=float, default=180)
a = p.parse_args()
if min(a.pairs, a.messages, a.connections, a.input_workers, a.frontend_capacity, a.frontend_max,
       a.dequeue_batch_size, a.worker_minimum, a.poll_us) <= 0:
    p.error('all workload sizes, offered rate, and timeout must be positive')
if a.messages < 2:
    p.error('--messages must be at least two for the achieved-rate check')
if not math.isfinite(a.offered_rate) or a.offered_rate <= 0:
    p.error('--offered-rate must be finite and positive')
if not math.isfinite(a.trial_timeout) or a.trial_timeout <= 0:
    p.error('--trial-timeout must be finite and positive')
minimum_payload = len('latency:') + len(str(a.messages - 1)) + 1 + 20 + 1
if a.payload < minimum_payload:
    p.error('--payload is too short for an exact latency ID/timestamp record')
for side in ('before', 'after'):
    if min(getattr(a, side + '_queue_size'), getattr(a, side + '_consumer_workers')) <= 0:
        p.error('side resources must be positive')
a.output.mkdir(parents=True, exist_ok=True)
output, harness, rows = a.output.resolve(), Path(__file__).resolve().parent, []


def state(path):
    try:
        answer = subprocess.run(['git', '-C', path, 'rev-parse', 'HEAD'], capture_output=True,
                                text=True, check=False)
        dirty = subprocess.run(['git', '-C', path, 'status', '--porcelain'], capture_output=True,
                               text=True, check=False)
    except OSError:
        return {'path': str(path), 'revision': None, 'dirty': None}
    return {'path': str(path), 'revision': answer.stdout.strip() if answer.returncode == 0 else None,
            'dirty': bool(dirty.stdout.strip()) if dirty.returncode == 0 else None}


def image_setup_timeout(deadline):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise TimeoutError('image setup exceeded --trial-timeout of %g seconds' % a.trial_timeout)
    return remaining


def resolve_image():
    deadline = time.monotonic() + a.trial_timeout
    command = ['docker', 'image', 'inspect', '--format', '{{.Id}}', a.image]
    try:
        inspected = subprocess.run(command, capture_output=True, text=True, check=False,
                                   timeout=image_setup_timeout(deadline))
        if inspected.returncode != 0:
            subprocess.run(['docker', 'pull', a.image], check=True, timeout=image_setup_timeout(deadline))
            inspected = subprocess.run(command, capture_output=True, text=True, check=True,
                                       timeout=image_setup_timeout(deadline))
    except subprocess.TimeoutExpired as error:
        raise TimeoutError('image setup exceeded --trial-timeout of %g seconds' % a.trial_timeout) from error
    image_id = inspected.stdout.strip()
    if not image_id:
        raise ValueError('image ID is empty')
    return image_id


def report(status, failure=None):
    accepted = status == 'completed' and all(r['before']['status'] == r['after']['status'] == 'completed' for r in rows)
    body = {'schema_version': 1, 'status': status, 'summary_accepted': accepted, 'timing_accepted': accepted,
            'latency_definition': 'same-process monotonic pre-framing-payload-timestamp-to-complete-file-line '
            'observation; polling included',
            'guardrail': 'each pair: after p99 <= before p99 + max(0.10 * before p99, 0.001 seconds)',
            'workload': {'messages': a.messages, 'connections': a.connections, 'input_workers': a.input_workers,
                         'offered_rate': a.offered_rate, 'poll_us': a.poll_us, 'omfile_flush_policy': a.flush_policy,
                         'frontend_capacity': a.frontend_capacity, 'frontend_max': a.frontend_max,
                         'dequeue_batch_size': a.dequeue_batch_size, 'worker_minimum': a.worker_minimum,
                         'per_side': {s: {'queue_size': getattr(a, s + '_queue_size'),
                                          'consumer_workers': getattr(a, s + '_consumer_workers'),
                                          'scope': getattr(a, s + '_scope')} for s in ('before', 'after')}},
            'image': {'reference': a.image, 'id': image},
            'builds': {s: state(getattr(a, s).resolve()) for s in ('before', 'after')},
            'harness': state(harness.parents[1]), 'requested_pairs': a.pairs,
            'completed_pairs': len(rows), 'pairs': rows, 'failure': failure}
    if accepted:
        margins = []
        for row in rows:
            before = row['before']['latency_ns']['p99'] / 1e9
            after = row['after']['latency_ns']['p99'] / 1e9
            allowance = max(.10 * before, .001)
            row['paired_p99'] = {'before_seconds': before, 'after_seconds': after,
                                 'allowance_seconds': allowance, 'margin_seconds': after - before,
                                 'guardrail_passed': after - before <= allowance}
            margins.append(after - before)
        median_margin = statistics.median(margins)
        body.update({'paired_margin_seconds': {'median': median_margin,
                     'mad': statistics.median(abs(value - median_margin) for value in margins)},
                     'guardrail_passed': all(row['paired_p99']['guardrail_passed'] for row in rows)})
        body['summary_accepted'] = body['timing_accepted'] = body['guardrail_passed']
    (output / 'result.json').write_text(json.dumps(body, indent=2) + '\n')


image = None
try:
    image = resolve_image()
    for pair in range(-1, a.pairs):
        row = {'pair': pair}
        for side in (('before', 'after') if pair % 2 == 0 else ('after', 'before')):
            name, build = '%02d-%s' % (pair + 1, side), getattr(a, side).resolve()
            metric, log = output / (name + '.json'), output / (name + '.log')
            container_name = 'rsyslog-queue-latency-%d-%d-%s' % (os.getpid(), pair + 1, side)
            command = ['docker', 'run', '--rm', '--name', container_name, '-u', '%d:%d' % (os.getuid(), os.getgid()),
                       '-v', '%s:/rsyslog' % build, '-v', '%s:/campaign:ro' % harness, '-v', '%s:/results' % output,
                       '-w', '/rsyslog/tests', '-e', 'BENCH_METRIC_FILE=/results/' + metric.name,
                       '-e', 'BENCH_MESSAGES=%d' % a.messages, '-e', 'BENCH_CONNECTIONS=%d' % a.connections,
                       '-e', 'BENCH_INPUT_WORKERS=%d' % a.input_workers, '-e', 'BENCH_OFFERED_RATE=%s' % a.offered_rate,
                       '-e', 'BENCH_PAYLOAD=%d' % a.payload,
                       '-e', 'BENCH_QUEUE_SIZE=%d' % getattr(a, side + '_queue_size'),
                       '-e', 'BENCH_CONSUMER_WORKERS=%d' % getattr(a, side + '_consumer_workers'),
                       '-e', 'BENCH_DEQUEUE_BATCH_SIZE=%d' % a.dequeue_batch_size,
                       '-e', 'BENCH_WORKER_MINIMUM=%d' % a.worker_minimum,
                       '-e', 'BENCH_SCOPE=%s' % getattr(a, side + '_scope'), '-e', 'BENCH_POLL_US=%d' % a.poll_us,
                       '-e', 'BENCH_FRONTEND_CAPACITY=%d' % a.frontend_capacity,
                       '-e', 'BENCH_FRONTEND_MAX=%d' % a.frontend_max,
                       '-e', 'BENCH_OMFILE_FLUSH_POLICY=%s' % a.flush_policy,
                       image, 'bash', '/campaign/trial-latency.sh']
            with log.open('w') as stream:
                try:
                    subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True,
                                   timeout=a.trial_timeout)
                except subprocess.TimeoutExpired:
                    subprocess.run(['docker', 'rm', '--force', container_name], stdout=stream,
                                   stderr=subprocess.STDOUT, check=False, timeout=30)
                    raise
            row[side] = json.loads(metric.read_text())
        if pair >= 0:
            rows.append(row)
            report('incomplete')
except (OSError, ValueError, subprocess.SubprocessError, json.JSONDecodeError) as error:
    report('incomplete', {'type': type(error).__name__, 'message': str(error)})
    raise
report('completed')
