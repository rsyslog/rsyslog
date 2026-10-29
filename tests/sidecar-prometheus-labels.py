#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check exact label semantics in file parsing and emitted Prometheus samples.

Expected Unicode values are literal independent oracles. The public Prometheus
parser first verifies each input fixture, then decodes the exporter's output;
matching only two parser results would not prove preservation of the labels.
No sockets, HTTP server, UDP listener, or external service is started.
"""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from prometheus_client import CollectorRegistry, generate_latest
from prometheus_client.parser import text_string_to_metric_families

# Importing the WSGI module creates an application. Force its file backend and
# make any accidental listener setup an immediate fixture error.
os.environ["IMPSTATS_MODE"] = "file"
os.environ["IMPSTATS_PATH"] = os.devnull
os.environ["IMPSTATS_FORMAT"] = "prometheus"
os.environ["LISTEN_PORT"] = "9898"

SOURCE = Path(__file__).resolve().parents[1] / "sidecar" / "rsyslog_exporter.py"
SPEC = importlib.util.spec_from_file_location("sidecar_exporter", SOURCE)
EXPORTER = importlib.util.module_from_spec(SPEC)
with mock.patch("socket.socket", side_effect=AssertionError("parser test must not open sockets")):
    SPEC.loader.exec_module(EXPORTER)


class PrometheusLabelsTest(unittest.TestCase):
    def samples(self, text):
        return [sample for family in text_string_to_metric_families(text) for sample in family.samples]

    def check_labels(self, encoded, expected):
        text = '# TYPE sidecar_sample gauge\nsidecar_sample{resource="%s",origin="core"} 7\n' % encoded
        labels = {"resource": expected, "origin": "core"}
        reference = self.samples(text)
        self.assertEqual(len(reference), 1)
        self.assertEqual(reference[0].labels, labels)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "metrics.prom"
            source.write_text(text, encoding="utf-8")
            parsed = EXPORTER.parse_prometheus_impstats(str(source))
            with self.subTest(oracle="direct"):
                self.assertEqual(len(parsed), 1)
                self.assertEqual(parsed[0].labels, labels)
                self.assertEqual((parsed[0].name, parsed[0].value, parsed[0].metric_type),
                                 ("sidecar_sample", 7.0, "gauge"))
            collector = EXPORTER.ImpstatsCollector(mode="file", file_path=str(source), format_type="prometheus")
            registry = CollectorRegistry()
            registry.register(collector)
            emitted = self.samples(generate_latest(registry).decode("utf-8"))
            with self.subTest(oracle="exported"):
                self.assertEqual(len(emitted), 1)
                self.assertEqual(emitted[0].labels, labels)
                self.assertEqual((emitted[0].name, emitted[0].value), ("sidecar_sample", 7.0))

    def test_label_values(self):
        cases = [
            ("plain", "plain"),
            ("", ""),
            ("Größe", "Größe"),
            ("a,b", "a,b"),
            (r'say \"hi\"', 'say "hi"'),
            (r"C:\\logs", "C:\\logs"),
            (r"top\nbottom", "top\nbottom"),
            ("a}b", "a}b"),
        ]
        for encoded, expected in cases:
            with self.subTest(encoded=encoded):
                self.check_labels(encoded, expected)

    def test_plain_samples_and_malformed_line(self):
        text = ("# TYPE sidecar_count counter\nsidecar_count 3\n"
                "not a valid metric\n"
                '# TYPE sidecar_level gauge\nsidecar_level{origin="core"} -2.5\n')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "controls.prom"
            source.write_text(text, encoding="utf-8")
            parsed = EXPORTER.parse_prometheus_impstats(str(source))
        self.assertEqual([(sample.name, sample.value, sample.labels, sample.metric_type) for sample in parsed],
                         [("sidecar_count", 3.0, {}, "counter"),
                          ("sidecar_level", -2.5, {"origin": "core"}, "gauge")])


if __name__ == "__main__":
    unittest.main()
