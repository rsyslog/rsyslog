#!/usr/bin/env python3
"""Alternate full-lifecycle, exact-delivery trials in two isolated builds."""
import argparse
import json
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
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
output = args.output.resolve()
harness = Path(__file__).resolve().parent
results = []
for pair in range(-1, args.pairs):
    sample = {'pair': pair}
    order = ['before', 'after'] if pair % 2 == 0 else ['after', 'before']
    for label in order:
        build = getattr(args, label).resolve()
        name = f'{pair + 1:02d}-{label}'
        metric = f'/results/{name}.ns'
        command = ['docker', 'run', '--rm', '-u', f'{os.getuid()}:{os.getgid()}',
                   '-v', f'{build}:/rsyslog', '-v', f'{harness}:/campaign:ro',
                   '-v', f'{output}:/results', '-e', f'BENCH_METRIC_FILE={metric}',
                   '-e', f'BENCH_MESSAGES={args.messages}', '-w', '/rsyslog/tests',
                   '-e', f'BENCH_INPUT_WORKERS={args.input_workers}',
                   '-e', f'BENCH_CONSUMER_WORKERS={args.consumer_workers}',
                   '-e', f'BENCH_CONNECTIONS={args.connections}', '-e', f'BENCH_PAYLOAD={args.payload}',
                   args.image, 'bash', f'/campaign/trial{"-multi" if args.workload == "multi" else ""}.sh']
        with (output / f'{name}.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        raw = (output / f'{name}.ns').read_text()
        metrics = json.loads(raw) if args.workload == 'multi' else {'lifecycle_ns': int(raw)}
        sample[label] = metrics.get('work_ns', metrics['lifecycle_ns']) / 1e9
        sample[label + '_metrics'] = {key.replace('_ns', '_seconds'): value / 1e9 for key, value in metrics.items()}
    sample['ratio'] = sample['after'] / sample['before']
    if pair >= 0:
        results.append(sample)
    print(json.dumps(sample), flush=True)
ratios = [row['ratio'] for row in results]
median = statistics.median(ratios)
report = {'schema_version': 1, 'messages': args.messages, 'workload': args.workload, 'pairs': results,
          'input_workers': args.input_workers if args.workload == 'multi' else 1,
          'consumer_workers': args.consumer_workers if args.workload == 'multi' else 4,
          'connections': args.connections if args.workload == 'multi' else 0,
          'median_after_before': median,
          'ratio_mad': statistics.median(abs(value - median) for value in ratios),
          'metric': 'generation plus drain seconds' if args.workload == 'multi' else 'full lifecycle seconds',
          'limitations': ['Non-exclusive host', 'Uncontrolled caches',
                          'Bounded JSON workload; exact delivery required; batch1024']}
(output / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report), flush=True)
