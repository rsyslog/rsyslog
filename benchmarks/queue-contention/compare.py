#!/usr/bin/env python3
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
parser.add_argument('--image', default='rsyslog/rsyslog_dev_base_ubuntu:26.04')
parser.add_argument('--trial-timeout', type=float, default=180,
                    help='maximum seconds allowed for each container trial')
args = parser.parse_args()
if args.pairs <= 0:
    parser.error('--pairs must be positive')
if args.messages <= 0:
    parser.error('--messages must be positive')
if args.workload == 'multi' and min(args.input_workers, args.consumer_workers,
                                    args.connections, args.payload) <= 0:
    parser.error('--input-workers, --consumer-workers, --connections, and --payload must be positive for multi')
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


image_id = None
builds = {label: checkout_state(getattr(args, label).resolve()) for label in ('before', 'after')}
workload_checkout = checkout_state(harness.parents[1])


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
            'consumer_workers': args.consumer_workers if args.workload == 'multi' else 4,
            'connections': args.connections if args.workload == 'multi' else 0,
            'payload': args.payload if args.workload == 'multi' else 0,
            'queue': {
                'type': 'FixedArray',
                'size': 32768,
                'dequeue_batch_size': 1024,
                'worker_thread_minimum_messages': 1024,
            },
        },
        'image': {'reference': args.image, 'id': image_id},
        'builds': builds,
        'trial_timeout_seconds': args.trial_timeout,
        'pairs': results,
        'completed_pairs': len(results),
        'metric': 'generation plus drain seconds' if args.workload == 'multi' else 'full lifecycle seconds',
        'limitations': ['Non-exclusive host', 'Uncontrolled caches',
                        'Bounded JSON workload; exact delivery required; batch1024'],
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
    image_id = subprocess.run(['docker', 'image', 'inspect', '--format', '{{.Id}}', args.image],
                              capture_output=True, check=True, text=True).stdout.strip()
    for pair in range(-1, args.pairs):
        sample = {'pair': pair}
        order = ['before', 'after'] if pair % 2 == 0 else ['after', 'before']
        for label in order:
            build = getattr(args, label).resolve()
            name = f'{pair + 1:02d}-{label}'
            container_name = f'rsyslog-queue-contention-{os.getpid()}-{pair + 1}-{label}'
            metric = f'/results/{name}.ns'
            command = [
                'docker', 'run', '--rm', '-u', f'{os.getuid()}:{os.getgid()}', '--name', container_name,
                '-v', f'{build}:/rsyslog', '-v', f'{harness}:/campaign:ro', '-v', f'{output}:/results',
                '-e', f'BENCH_METRIC_FILE={metric}', '-e', f'BENCH_MESSAGES={args.messages}',
                '-w', '/rsyslog/tests', '-e', f'BENCH_INPUT_WORKERS={args.input_workers}',
                '-e', f'BENCH_CONSUMER_WORKERS={args.consumer_workers}',
                '-e', f'BENCH_CONNECTIONS={args.connections}', '-e', f'BENCH_PAYLOAD={args.payload}',
                image_id, 'bash', f'/campaign/trial{"-multi" if args.workload == "multi" else ""}.sh',
            ]
            with (output / f'{name}.log').open('w') as log:
                try:
                    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True,
                                   timeout=args.trial_timeout)
                except subprocess.TimeoutExpired:
                    subprocess.run(['docker', 'rm', '--force', container_name], stdout=log,
                                   stderr=subprocess.STDOUT, check=False)
                    raise
            raw = (output / f'{name}.ns').read_text()
            metrics = json.loads(raw) if args.workload == 'multi' else {'lifecycle_ns': int(raw)}
            sample[label] = metrics.get('work_ns', metrics['lifecycle_ns']) / 1e9
            sample[label + '_metrics'] = {
                key.replace('_ns', '_seconds'): value / 1e9 for key, value in metrics.items()
            }
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
