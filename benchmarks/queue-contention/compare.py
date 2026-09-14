#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Adiscon GmbH
# This file is part of rsyslog.
# Released under ASL 2.0
"""Alternate full-lifecycle, exact-delivery trials in two isolated builds."""
import argparse
import json
import math
import os
from pathlib import Path
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--before', required=True, type=Path)
parser.add_argument('--after', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--pairs', type=int, default=11)
parser.add_argument('--messages', type=int, default=100000)
parser.add_argument('--workload', choices=['lifecycle', 'multi'], default='lifecycle')
parser.add_argument('--input-workers', type=int, default=8)
parser.add_argument('--consumer-workers', type=int, default=4)
parser.add_argument('--connections', type=int, default=16)
parser.add_argument('--payload', type=int, default=512)
parser.add_argument('--queue-size', type=int, default=32768)
parser.add_argument('--before-queue-size', type=int)
parser.add_argument('--after-queue-size', type=int)
parser.add_argument('--dequeue-batch-size', type=int, default=1024)
parser.add_argument('--worker-minimum', type=int, default=1024)
parser.add_argument('--producer-mode', choices=['balanced', 'skew'], default='balanced')
parser.add_argument('--frontend-capacity', type=int,
                    help='deprecated alias for reporting a local frontend count; use per-side max-frontends')
parser.add_argument('--before-scope', choices=['global', 'local'], default='global')
parser.add_argument('--after-scope', choices=['global', 'local'], default='global')
parser.add_argument('--before-frontend-size', type=int)
parser.add_argument('--after-frontend-size', type=int)
parser.add_argument('--before-max-frontends', type=int)
parser.add_argument('--after-max-frontends', type=int)
parser.add_argument('--print-configuration', action='store_true',
                    help='validate and print resolved per-side configuration without starting Docker')
parser.add_argument('--before-consumer-workers', type=int)
parser.add_argument('--after-consumer-workers', type=int)
parser.add_argument('--impstats', action='store_true',
                    help='capture raw queue impstats diagnostics outside timed interpretation')
parser.add_argument('--image', default='rsyslog/rsyslog_dev_base_ubuntu:26.04')
parser.add_argument('--trial-timeout', type=float, default=180,
                    help='maximum seconds allowed for each container trial')
args = parser.parse_args()
if args.pairs <= 0:
    parser.error('--pairs must be positive')
if args.messages <= 0:
    parser.error('--messages must be positive')
if args.workload == 'multi' and min(args.input_workers, args.consumer_workers,
                                    args.connections, args.payload, args.queue_size,
                                    args.dequeue_batch_size, args.worker_minimum) <= 0:
    parser.error('multi workload sizes, workers, connections, and payload must be positive')
if args.frontend_capacity is not None and args.frontend_capacity <= 0:
    parser.error('--frontend-capacity must be positive')
for label in ('before', 'after'):
    for field in ('queue_size', 'consumer_workers'):
        value = getattr(args, f'{label}_{field}')
        if value is not None and value <= 0:
            parser.error(f'--{label}-{field.replace("_", "-")} must be positive')
    scope = getattr(args, f'{label}_scope')
    frontend_size = getattr(args, f'{label}_frontend_size')
    max_frontends = getattr(args, f'{label}_max_frontends')
    if scope == 'local' and (frontend_size is None or max_frontends is None):
        parser.error(f'--{label}-scope local requires --{label}-frontend-size and --{label}-max-frontends')
    if scope == 'global' and (frontend_size is not None or max_frontends is not None):
        parser.error(f'--{label}-frontend-size and --{label}-max-frontends require --{label}-scope local')
    if frontend_size is not None and frontend_size <= 0:
        parser.error(f'--{label}-frontend-size must be positive')
    if max_frontends is not None and max_frontends <= 0:
        parser.error(f'--{label}-max-frontends must be positive')
active_connections = 1 if args.producer_mode == 'skew' else args.connections
if args.workload == 'multi' and args.messages % active_connections != 0:
    parser.error('--messages must be divisible by active connections')
if not math.isfinite(args.trial_timeout) or args.trial_timeout <= 0:
    parser.error('--trial-timeout must be finite and positive')
args.output.mkdir(parents=True, exist_ok=True)
output = args.output.resolve()
harness = Path(__file__).resolve().parent
results = []


def checkout_state(path):
    """Record checkout state without treating it as binary-build provenance."""
    revision = subprocess.run(['git', '-C', path, 'rev-parse', 'HEAD'], capture_output=True,
                              check=False, text=True)
    dirty = subprocess.run(['git', '-C', path, 'status', '--porcelain'], capture_output=True,
                           check=False, text=True)
    return {'path': str(path), 'revision': revision.stdout.strip() if revision.returncode == 0 else None,
            'dirty': bool(dirty.stdout.strip()) if dirty.returncode == 0 else None}


def resolve_image():
    command = ['docker', 'image', 'inspect', '--format', '{{.Id}}', args.image]
    inspected = subprocess.run(command, capture_output=True, check=False, text=True,
                               timeout=args.trial_timeout)
    if inspected.returncode != 0:
        subprocess.run(['docker', 'pull', args.image], check=True, timeout=args.trial_timeout)
        inspected = subprocess.run(command, capture_output=True, check=True, text=True,
                                   timeout=args.trial_timeout)
    image_id = inspected.stdout.strip()
    if not image_id:
        raise ValueError('docker image inspect returned an empty image ID')
    return image_id


def read_metrics(path):
    raw = path.read_text()
    if args.workload == 'lifecycle':
        metrics = {'lifecycle_ns': int(raw)}
    else:
        metrics = json.loads(raw)
        if not isinstance(metrics, dict):
            raise ValueError(f'{path} must contain a JSON object')
        required = ('lifecycle_ns', 'work_ns', 'generator_ns', 'drain_ns',
                    'receiver_line_barrier_ns', 'post_shutdown_exact_verification_ns')
        if any(name not in metrics for name in required):
            raise ValueError(f'{path} is missing required metrics')
    if any(not isinstance(value, int) or isinstance(value, bool) or value < 0 for value in metrics.values()):
        raise ValueError(f'{path} contains invalid metric values')
    return metrics


image_id = None
builds = {label: checkout_state(getattr(args, label).resolve()) for label in ('before', 'after')}
workload_checkout = checkout_state(harness.parents[1])


def build_configuration(label):
    """Resolve per-build resource bounds while leaving legacy global configs unchanged."""
    queue_size = getattr(args, f'{label}_queue_size')
    consumer_workers = getattr(args, f'{label}_consumer_workers')
    scope = getattr(args, f'{label}_scope')
    frontend_size = getattr(args, f'{label}_frontend_size')
    max_frontends = getattr(args, f'{label}_max_frontends')
    backend_queue_size = args.queue_size if queue_size is None else queue_size
    total_slot_bound = backend_queue_size
    if scope == 'local':
        total_slot_bound += max_frontends * (frontend_size + args.dequeue_batch_size)
    return {
        'scope': scope,
        'queue_size': backend_queue_size,
        'consumer_workers': args.consumer_workers if consumer_workers is None else consumer_workers,
        'dequeue_batch_size': args.dequeue_batch_size,
        'worker_minimum_messages': args.worker_minimum,
        'frontend_size': frontend_size,
        'max_frontends': max_frontends,
        'total_slot_bound': total_slot_bound,
    }


per_build_configuration = {label: build_configuration(label) for label in ('before', 'after')}
if args.print_configuration:
    print(json.dumps({'per_build_configuration': per_build_configuration}, sort_keys=True))
    raise SystemExit(0)


def write_report(status, failure=None):
    ratios = [row['ratio'] for row in results]
    report = {
        'schema_version': 2,
        'status': status,
        'requested_pairs': args.pairs,
        'workload': {
            'name': args.workload,
            'script': f'trial{"-multi" if args.workload == "multi" else ""}.sh',
            'revision': workload_checkout['revision'],
            'dirty': workload_checkout['dirty'],
            'messages': args.messages,
            'input_workers': args.input_workers if args.workload == 'multi' else 1,
            'configured_consumer_workers': args.consumer_workers if args.workload == 'multi' else 4,
            'configured_connections': args.connections if args.workload == 'multi' else 0,
            'active_connections': active_connections if args.workload == 'multi' else 0,
            'producer_mode': args.producer_mode if args.workload == 'multi' else 'single',
            'frontend_capacity': args.frontend_capacity,
            'impstats_enabled': args.impstats,
            'actual_batch_distributions': 'not_observed',
            'payload': args.payload if args.workload == 'multi' else 0,
            'queue_type': 'FixedArray',
            'per_build_configuration': per_build_configuration,
        },
        'image': {'reference': args.image, 'id': image_id},
        'builds': builds,
        'trial_timeout_seconds': args.trial_timeout,
        'pairs': results,
        'completed_pairs': len(results),
        'metric': ('sender generation through receiver line barrier seconds; '
                   'post-shutdown exact-ID verification is required and reported separately')
        if args.workload == 'multi' else 'full lifecycle seconds',
        'limitations': ['Non-exclusive host', 'Uncontrolled caches',
                        'Bounded JSON workload; exact delivery required',
                        'Configured batches are not actual-batch distributions'],
        'timing_accepted': status == 'completed' and not args.impstats,
        'summary_accepted': status == 'completed' and not args.impstats,
    }
    if status == 'completed':
        median = statistics.median(ratios)
        report['median_after_before'] = median
        report['ratio_mad'] = statistics.median(abs(value - median) for value in ratios)
    else:
        report['summary_accepted'] = False
    if failure is not None:
        report['failure'] = failure
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


try:
    image_id = resolve_image()
    for pair in range(-1, args.pairs):
        sample = {'pair': pair}
        order = ['before', 'after'] if pair % 2 == 0 else ['after', 'before']
        for label in order:
            build = getattr(args, label).resolve()
            configuration = per_build_configuration[label]
            name = f'{pair + 1:02d}-{label}'
            container_name = f'rsyslog-queue-contention-{os.getpid()}-{pair + 1}-{label}'
            metric = f'/results/{name}.ns'
            impstats_metric = f'/results/{name}.impstats.json'
            command = [
                'docker', 'run', '--rm', '-u', f'{os.getuid()}:{os.getgid()}', '--name', container_name,
                '-v', f'{build}:/rsyslog', '-v', f'{harness}:/campaign:ro', '-v', f'{output}:/results',
                '-e', f'BENCH_METRIC_FILE={metric}', '-e', f'BENCH_MESSAGES={args.messages}',
                '-w', '/rsyslog/tests', '-e', f'BENCH_INPUT_WORKERS={args.input_workers}',
                '-e', f'BENCH_CONSUMER_WORKERS={configuration["consumer_workers"]}',
                '-e', f'BENCH_CONNECTIONS={args.connections}', '-e', f'BENCH_PAYLOAD={args.payload}',
                '-e', f'BENCH_QUEUE_SIZE={configuration["queue_size"]}',
                '-e', f'BENCH_DEQUEUE_BATCH_SIZE={configuration["dequeue_batch_size"]}',
                '-e', f'BENCH_WORKER_MINIMUM={configuration["worker_minimum_messages"]}',
                '-e', f'BENCH_PRODUCER_MODE={args.producer_mode}',
                '-e', f'BENCH_IMPSTATS={"yes" if args.impstats else "no"}',
                '-e', f'BENCH_IMPSTATS_FILE={impstats_metric}',
                image_id, 'bash', f'/campaign/trial{"-multi" if args.workload == "multi" else ""}.sh',
            ]
            if configuration['scope'] == 'local':
                command[command.index(image_id):command.index(image_id)] = [
                    '-e', 'BENCH_SCOPE=local',
                    '-e', f'BENCH_FRONTEND_SIZE={configuration["frontend_size"]}',
                    '-e', f'BENCH_MAX_FRONTENDS={configuration["max_frontends"]}',
                ]
            with (output / f'{name}.log').open('w') as log:
                try:
                    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True,
                                   timeout=args.trial_timeout)
                except subprocess.TimeoutExpired:
                    subprocess.run(['docker', 'rm', '--force', container_name], stdout=log,
                                   stderr=subprocess.STDOUT, check=False, timeout=30)
                    raise
            metrics = read_metrics(output / f'{name}.ns')
            primary = 'work_ns' if args.workload == 'multi' else 'lifecycle_ns'
            if metrics[primary] == 0:
                raise ValueError(f'{name}.ns has a non-positive primary metric')
            sample[label] = metrics[primary] / 1e9
            sample[label + '_metrics'] = {
                key.replace('_ns', '_seconds'): value / 1e9 for key, value in metrics.items()
            }
            sample[label + '_configuration'] = configuration
            if args.impstats:
                sample[label + '_impstats_file'] = f'{name}.impstats.json'
        sample['ratio'] = sample['after'] / sample['before']
        if pair >= 0:
            results.append(sample)
            write_report('incomplete')
        print(json.dumps(sample), flush=True)
except (subprocess.SubprocessError, OSError, ValueError, json.JSONDecodeError) as error:
    write_report('incomplete', {'type': type(error).__name__, 'message': str(error)})
    raise

report = write_report('completed')
print(json.dumps(report), flush=True)
