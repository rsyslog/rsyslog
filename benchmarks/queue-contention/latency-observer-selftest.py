#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Deterministic oracle tests for latency-observer output and validity rules."""
import importlib.util
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('observer', Path(__file__).with_name('latency-observer.py'))
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


def payload_line(identifier, timestamp, payload=32):
    record = 'latency:%d:%d' % (identifier, timestamp)
    return record + '|' + 'x' * (payload - len(record) - 1)


def read_fixture(text, expected, payload=32):
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / 'sink'
        output.write_text(text)
        result = observer.Observer(output, expected, payload, 1)
        thread = __import__('threading').Thread(target=result.run)
        thread.start()
        deadline = time.monotonic() + 1
        while len(result.seen) + result.invalid + result.duplicates < len(text.splitlines()):
            assert time.monotonic() < deadline
            time.sleep(.001)
        result.stop()
        thread.join()
        return result


good = read_fixture(payload_line(0, 1) + '\n' + payload_line(1, 2) + '\n', {0, 1})
assert set(good.seen) == {0, 1} and not good.invalid and not good.duplicates
missing = read_fixture(payload_line(0, 1) + '\n', {0, 1})
assert set(missing.expected) - set(missing.seen) == {1}
duplicate = read_fixture(payload_line(0, 1) + '\n' + payload_line(0, 1) + '\n', {0})
assert duplicate.duplicates == 1
invalid = read_fixture('not-a-latency-line\n', {0})
assert invalid.invalid == 1
truncated = read_fixture(payload_line(0, 1)[:-1] + '\n', {0})
assert truncated.invalid == 1
assert not observer.rate_is_valid(100, 90, 2.0)  # fixed offered-rate invalidation threshold

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'sink'
    output.write_text(payload_line(0, 1) + '\n' + payload_line(0, 1) + '\n')
    final = observer.final_oracle(output, {0}, 32, {0: 1})
    assert final['duplicates'] == 1
    output.write_text(payload_line(0, 1) + '\nlate-fragment')
    final = observer.final_oracle(output, {0}, 32, {0: 1})
    assert final['trailing_bytes'] == len('late-fragment')
    expected, result = Path(directory) / 'expected', Path(directory) / 'result.json'
    expected.write_text('0 1\n')
    result.write_text(json.dumps({'status': 'completed'}))
    finalized = subprocess.run([sys.executable, str(Path(__file__).with_name('latency-observer.py')), '--finalize',
                                '--output', str(output), '--result', str(result), '--expected', str(expected),
                                '--messages', '1', '--payload', '32'], capture_output=True, text=True, check=False)
    assert finalized.returncode != 0
    final = json.loads(result.read_text())['final_oracle']
    assert json.loads(result.read_text())['status'] == 'invalid' and final['trailing_bytes'] == len('late-fragment')
    output.write_text(payload_line(0, 2) + '\n')
    final = observer.final_oracle(output, {0}, 32, {0: 1})
    assert final['timestamp_mismatch'] == 1
    expected.write_text('0 1\n')
    timestamps = observer.load_expected(expected, {0, 1})
    output.write_text(payload_line(0, 1) + '\n')
    final = observer.final_oracle(output, {0, 1}, 32, timestamps)
    assert final['missing'] == [1] and not final['timestamp_mismatch']


with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'sink'
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen()

    def sink():
        with listener, output.open('wb') as file:
            connection, _ = listener.accept()
            with connection:
                pending = b''
                while data := connection.recv(4096):
                    pending += data
                    while b' ' in pending:
                        length, message = pending.split(b' ', 1)
                        if len(message) < int(length):
                            break
                        message, pending = message[:int(length)], message[int(length):]
                        file.write(message.rsplit(b' - ', 1)[1] + b'\n')
                        file.flush()
                assert not pending

    server = threading.Thread(target=sink)
    server.start()
    expected = Path(directory) / 'expected'
    result = observer.run(SimpleNamespace(host='127.0.0.1', port=listener.getsockname()[1], output=output,
                          expected=expected,
                          messages=4, connections=1, rate=100, id_start=0, poll_us=100, warmup_ms=5,
                          completion_timeout=2, connect_timeout=2, max_rate_drift_percent=100,
                          max_lateness_us=100000, max_poll_gap_us=100000, payload=128))
    server.join()
    final = observer.final_oracle(output, {0, 1, 2, 3}, 128,
                                  observer.load_expected(expected, {0, 1, 2, 3}))
    assert result['status'] == 'completed' and result['oracle']['received'] == 4
    assert not (final['missing'] or final['duplicates'] or final['invalid_output']
                or final['timestamp_mismatch'] or final['trailing_bytes'])
