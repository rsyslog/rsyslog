#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Deterministic oracle tests for latency-observer output and validity rules."""
import importlib.util
from pathlib import Path
import socket
import tempfile
import threading
import time
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('observer', Path(__file__).with_name('latency-observer.py'))
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


def read_fixture(text, expected):
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / 'sink'
        output.write_text(text)
        result = observer.Observer(output, expected, 1)
        thread = __import__('threading').Thread(target=result.run)
        thread.start()
        time.sleep(.01)
        result.stop()
        thread.join()
        return result


good = read_fixture('latency:0:1\nlatency:1:2\n', {0, 1})
assert set(good.seen) == {0, 1} and not good.invalid and not good.duplicates
missing = read_fixture('latency:0:1\n', {0, 1})
assert set(missing.expected) - set(missing.seen) == {1}
duplicate = read_fixture('latency:0:1\nlatency:0:1\n', {0})
assert duplicate.duplicates == 1
invalid = read_fixture('not-a-latency-line\n', {0})
assert invalid.invalid == 1
assert not observer.rate_is_valid(100, 90, 2.0)  # fixed offered-rate invalidation threshold


with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'sink'
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen()

    def sink():
        with listener, output.open('wb') as file:
            connection, _ = listener.accept()
            with connection:
                while data := connection.recv(4096):
                    file.write(data)
                    file.flush()

    server = threading.Thread(target=sink)
    server.start()
    result = observer.run(SimpleNamespace(host='127.0.0.1', port=listener.getsockname()[1], output=output,
                          messages=4, connections=1, rate=100, id_start=0, poll_us=100, warmup_ms=5,
                          completion_timeout=2, connect_timeout=2, max_rate_drift_percent=20,
                          max_lateness_ms=100, max_poll_gap_ms=100))
    server.join()
    assert result['status'] == 'completed' and result['oracle']['received'] == 4
