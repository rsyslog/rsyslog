#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
#
# Copyright 2025-2026 Rainer Gerhards and Adiscon GmbH.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
rsyslog Prometheus Exporter

A lightweight sidecar process that reads rsyslog impstats files and exposes
metrics in Prometheus format via an HTTP endpoint.

Supports multiple impstats formats:
- json: Standard JSON format
- prometheus: Native Prometheus exposition format (rsyslog 8.2312.0+)
- cee: CEE/Lumberjack format with @cee cookie

Configuration via environment variables:
- IMPSTATS_MODE: Input mode: 'file' or 'udp' (default: udp)
- IMPSTATS_PATH: Path to impstats file when mode=file (default: /var/log/rsyslog/impstats.json)
- IMPSTATS_FORMAT: Format of impstats (json, prometheus, cee; default: json)
- IMPSTATS_UDP_PORT: UDP port to listen on when mode=udp (default: 19090)
- IMPSTATS_UDP_ADDR: UDP bind address when mode=udp (default: 127.0.0.1 - loopback only)
- STATS_COMPLETE_TIMEOUT: Seconds to wait for burst completion in UDP mode (default: 5)
- LISTEN_ADDR: Address to bind HTTP server (default: 127.0.0.1 - loopback only for security)
- LISTEN_PORT: Port for HTTP server (default: 9898)
- LOG_LEVEL: Logging level (DEBUG, INFO, WARNING, ERROR; default: INFO)

Security configuration (UDP mode):
- ALLOWED_UDP_SOURCES: Comma-separated list of allowed source IPs (default: empty = allow all)
- MAX_UDP_MESSAGE_SIZE: Maximum UDP packet size in bytes (default: 65535)
- MAX_BURST_BUFFER_LINES: Maximum lines in burst buffer (default: 10000, prevents DoS)

Security Notes:
- IMPSTATS_UDP_ADDR: Defaults to 127.0.0.1 (loopback). Use this for same-host rsyslog.
- LISTEN_ADDR: Defaults to 127.0.0.1 (loopback) for security. Set to:
  - 127.0.0.1: Local Prometheus only (most secure)
  - Specific IP: Bind to VPN/internal network interface
  - 0.0.0.0: All interfaces (use with firewall rules)
- ALLOWED_UDP_SOURCES: Enable if UDP listener must bind to 0.0.0.0 (e.g., containers)
"""

import json
import logging
import os
import re
import socket
import sys
import threading
import time
from collections import defaultdict
from typing import Dict, List, Optional

from prometheus_client import generate_latest
from prometheus_client.core import (
    CollectorRegistry,
    CounterMetricFamily,
    GaugeMetricFamily,
)
from werkzeug.serving import run_simple
from werkzeug.wrappers import Request, Response


def _int_env(name: str, default: str) -> int:
    value = os.getenv(name, default)
    try:
        return int(value)
    except ValueError as exc:
        print(
            f"FATAL: {name} must be an integer, got '{value}'.",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc


def _float_env(name: str, default: str) -> float:
    value = os.getenv(name, default)
    try:
        return float(value)
    except ValueError as exc:
        print(
            f"FATAL: {name} must be a number, got '{value}'.",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc


# Configuration from environment
IMPSTATS_MODE = os.getenv("IMPSTATS_MODE", "udp").lower()
IMPSTATS_PATH = os.getenv("IMPSTATS_PATH", "/var/log/rsyslog/impstats.json")
IMPSTATS_FORMAT = os.getenv("IMPSTATS_FORMAT", "json").lower()
IMPSTATS_UDP_PORT = _int_env("IMPSTATS_UDP_PORT", "19090")
IMPSTATS_UDP_ADDR = os.getenv("IMPSTATS_UDP_ADDR", "127.0.0.1")
STATS_COMPLETE_TIMEOUT = _float_env("STATS_COMPLETE_TIMEOUT", "5")
LISTEN_ADDR = os.getenv("LISTEN_ADDR", "127.0.0.1")  # Changed default to loopback for security
LISTEN_PORT = _int_env("LISTEN_PORT", "9898")
LOG_LEVEL = os.getenv("LOG_LEVEL", "INFO").upper()

# Security limits
MAX_UDP_MESSAGE_SIZE = _int_env("MAX_UDP_MESSAGE_SIZE", "65535")  # Max UDP packet size
MAX_BURST_BUFFER_LINES = _int_env("MAX_BURST_BUFFER_LINES", "10000")  # Prevent memory exhaustion
ALLOWED_UDP_SOURCES = os.getenv("ALLOWED_UDP_SOURCES", "")  # Comma-separated IPs, empty = allow all

# Logging setup
logging.basicConfig(
    level=getattr(logging, LOG_LEVEL, logging.INFO),
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger(__name__)


class Metric:
    """Internal representation of a metric."""
    
    def __init__(self, name: str, value: float, labels: Dict[str, str], metric_type: str = "gauge"):
        self.name = name
        self.value = value
        self.labels = labels
        self.metric_type = metric_type  # "gauge" or "counter"
    
    def __repr__(self):
        return f"Metric({self.name}={self.value}, labels={self.labels}, type={self.metric_type})"


def sanitize_metric_name(name: str) -> str:
    """
    Sanitize metric name to conform to Prometheus naming conventions.
    - Replace invalid characters with underscores
    - Ensure it starts with a letter or underscore
    - Convert to lowercase
    """
    # Replace invalid characters
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    # Ensure starts with letter or underscore
    if name and not re.match(r'^[a-zA-Z_]', name):
        name = '_' + name
    return name.lower()


def sanitize_label_name(name: str) -> str:
    """Sanitize label name for Prometheus."""
    name = re.sub(r'[^a-zA-Z0-9_]', '_', name)
    if name and not re.match(r'^[a-zA-Z_]', name):
        name = '_' + name
    return name


COUNTER_KEYS = {
    "processed", "failed", "submitted", "utime", "stime", "resumed",
    "enqueued", "discarded.full", "discarded.nf", "bytes.rcvd", "bytes.sent",
}
COUNTER_PREFIXES = ("called.",)
COUNTER_SUFFIXES = (".rcvd", ".sent", ".enqueued")


def build_base_labels(origin: str, name: str) -> Dict[str, str]:
    return {
        "rsyslog_component": origin,
        "rsyslog_resource": sanitize_label_name(name),
    }


def build_metric_name(origin: str, key: str) -> str:
    return f"rsyslog_{sanitize_metric_name(origin)}_{sanitize_metric_name(key)}"


def is_counter_key(key: str) -> bool:
    return (
        key in COUNTER_KEYS or
        key.startswith(COUNTER_PREFIXES) or
        key.endswith(COUNTER_SUFFIXES)
    )


def parse_numeric_value(value) -> Optional[float]:
    try:
        return float(value)
    except (ValueError, TypeError):
        return None


def parse_json_object(obj: Dict[str, object]) -> List[Metric]:
    metrics: List[Metric] = []

    name = obj.get("name", "unknown")
    origin = obj.get("origin", "unknown")
    base_labels = build_base_labels(origin, name)

    for key, value in obj.items():
        if key in ("name", "origin"):
            continue

        numeric_value = parse_numeric_value(value)
        if numeric_value is None:
            continue

        metric_type = "counter" if is_counter_key(key) else "gauge"
        metric_name = build_metric_name(origin, key)

        metrics.append(Metric(
            name=metric_name,
            value=numeric_value,
            labels=base_labels.copy(),
            metric_type=metric_type,
        ))

    return metrics


def parse_json_impstats(file_path: str) -> List[Metric]:
    """
    Parse rsyslog impstats in JSON format from a file.
    
    Expected format (one JSON object per line):
    {"name":"global","origin":"core","utime":"123456","stime":"78901",...}
    {"name":"action 0","origin":"core.action","processed":"1000","failed":"5",...}
    """
    metrics = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue
                
                metrics.extend(parse_json_line(line, line_num))
    
    except FileNotFoundError:
        logger.error(f"Impstats file not found: {file_path}")
    except Exception as e:
        logger.error(f"Error parsing JSON impstats: {e}", exc_info=True)
    
    return metrics


def parse_json_line(line: str, line_num: int = 0) -> List[Metric]:
    """
    Parse a single JSON line from impstats into metrics.
    Can be used by both file and UDP parsers.
    """
    try:
        obj = json.loads(line)
    except json.JSONDecodeError as e:
        logger.warning(f"Failed to parse JSON at line {line_num}: {e}")
        return []

    if not isinstance(obj, dict):
        logger.warning(f"Unexpected JSON structure at line {line_num}: {type(obj).__name__}")
        return []

    return parse_json_object(obj)


def parse_json_lines(lines: List[str]) -> List[Metric]:
    """Parse multiple JSON lines (e.g., from UDP burst)."""
    metrics = []
    for i, line in enumerate(lines):
        line = line.strip()
        if line:
            metrics.extend(parse_json_line(line, i + 1))
    return metrics


def parse_prometheus_impstats(file_path: str) -> List[Metric]:
    """
    Parse rsyslog impstats in native Prometheus format.
    
    This format is already Prometheus exposition format, so we parse it
    and convert to our internal representation for consistency.
    
    Example format:
    # TYPE rsyslog_global_utime counter
    rsyslog_global_utime{origin="core"} 123456
    """
    metrics = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            current_type = "gauge"
            
            for line in f:
                line = line.strip()
                
                # Skip empty lines and help comments
                if not line or line.startswith("# HELP"):
                    continue
                
                # Parse TYPE directive
                if line.startswith("# TYPE"):
                    parts = line.split()
                    if len(parts) >= 4:
                        current_type = parts[3]  # gauge, counter, etc.
                    continue
                
                # Skip other comments
                if line.startswith("#"):
                    continue
                
                # Parse metric line: metric_name{labels} value
                match = re.match(r'^([a-zA-Z_:][a-zA-Z0-9_:]*)\s*(\{[^}]*\})?\s+([0-9.eE+\-]+)', line)
                if not match:
                    continue
                
                metric_name = match.group(1)
                labels_str = match.group(2) or "{}"
                try:
                    value = float(match.group(3))
                except ValueError:
                    continue
                
                # Parse labels
                labels = {}
                if labels_str != "{}":
                    labels_str = labels_str.strip("{}")
                    for label_pair in labels_str.split(","):
                        if "=" in label_pair:
                            k, v = label_pair.split("=", 1)
                            labels[k.strip()] = v.strip().strip('"')
                
                metrics.append(Metric(
                    name=metric_name,
                    value=value,
                    labels=labels,
                    metric_type="counter" if current_type == "counter" else "gauge",
                ))
    
    except FileNotFoundError:
        logger.error(f"Impstats file not found: {file_path}")
    except Exception as e:
        logger.error(f"Error parsing Prometheus impstats: {e}", exc_info=True)
    
    return metrics


def parse_cee_impstats(file_path: str) -> List[Metric]:
    """
    Parse rsyslog impstats in CEE/Lumberjack format.
    
    Format: Lines starting with "@cee:" followed by JSON.
    Example: @cee:{"name":"global","origin":"core","utime":"123456"}
    
    Falls back to plain JSON parsing if no @cee cookie found.
    """
    metrics = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue
                
                # Check for @cee cookie
                if line.startswith("@cee:"):
                    json_str = line[5:].strip()
                else:
                    # Fallback: treat as plain JSON
                    json_str = line
                
                try:
                    obj = json.loads(json_str)
                except json.JSONDecodeError as e:
                    logger.warning(f"Failed to parse CEE JSON at line {line_num}: {e}")
                    continue

                if not isinstance(obj, dict):
                    logger.warning(
                        f"Unexpected CEE JSON structure at line {line_num}: {type(obj).__name__}"
                    )
                    continue

                metrics.extend(parse_json_object(obj))
    
    except FileNotFoundError:
        logger.error(f"Impstats file not found: {file_path}")
    except Exception as e:
        logger.error(f"Error parsing CEE impstats: {e}", exc_info=True)
    
    return metrics


class UdpStatsListener:
    """
    UDP listener for receiving impstats messages from rsyslog.
    
    Handles burst reception with completion timeout:
    - Receives all messages in a burst
    - Waits STATS_COMPLETE_TIMEOUT seconds after last message
    - Then updates metrics atomically
    """
    
    def __init__(self, addr: str, port: int, format_type: str, completion_timeout: float,
                 allowed_sources: List[str] = None):
        self.addr = addr
        self.port = port
        self.format_type = format_type
        self.completion_timeout = completion_timeout
        self.allowed_sources = allowed_sources or []  # Empty list = allow all
        self.sock = None
        self.running = False
        self.thread = None
        
        # Metrics storage
        self.cached_metrics: List[Metric] = []
        self.metrics_lock = threading.Lock()
        self.parse_count = 0
        
        # Burst handling
        self.burst_buffer: List[str] = []
        self.last_receive_time = 0
        self.dropped_messages = 0  # Track dropped messages for security monitoring
        
        # Select parser for lines
        if format_type == "json":
            self.line_parser = parse_json_lines
        elif format_type == "cee":
            self.line_parser = self._parse_cee_lines
        else:
            logger.warning(f"UDP mode does not support format '{format_type}', using json")
            self.line_parser = parse_json_lines
    
    def _parse_cee_lines(self, lines: List[str]) -> List[Metric]:
        """Parse CEE format lines."""
        json_lines = []
        for line in lines:
            if line.startswith("@cee:"):
                json_lines.append(line[5:].strip())
            else:
                json_lines.append(line)
        return parse_json_lines(json_lines)
    
    def start(self):
        """Start the UDP listener in a background thread."""
        if self.running:
            return
        
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind((self.addr, self.port))
            self.sock.settimeout(1.0)  # 1 second timeout for receive
            
            self.running = True
            self.thread = threading.Thread(target=self._listen_loop, daemon=True)
            self.thread.start()
            
            logger.info(f"UDP listener started on {self.addr}:{self.port}")
        except Exception as e:
            logger.error(f"Failed to start UDP listener: {e}", exc_info=True)
            raise
    
    def stop(self):
        """Stop the UDP listener."""
        self.running = False
        if self.thread:
            self.thread.join(timeout=5)
        if self.sock:
            self.sock.close()
        logger.info("UDP listener stopped")
    
    def _listen_loop(self):
        """Main listening loop - runs in background thread."""
        logger.debug("UDP listener loop started")
        
        while self.running:
            try:
                # Try to receive data
                try:
                    data, addr = self.sock.recvfrom(MAX_UDP_MESSAGE_SIZE)
                    if data:
                        # Source IP filtering
                        source_ip = addr[0]
                        if self.allowed_sources and source_ip not in self.allowed_sources:
                            logger.warning(f"Rejected UDP packet from unauthorized source: {source_ip}")
                            self.dropped_messages += 1
                            continue
                        
                        message = data.decode('utf-8', errors='ignore')
                        self._handle_message(message)
                except socket.timeout:
                    # Check if burst is complete
                    self._check_burst_completion()
                    continue
                
            except Exception as e:
                if self.running:
                    logger.error(f"Error in UDP listener: {e}", exc_info=True)
        
        logger.debug("UDP listener loop ended")
    
    def _handle_message(self, message: str):
        """Handle received UDP message (may contain multiple lines)."""
        lines = message.splitlines()
        
        with self.metrics_lock:
            # Prevent buffer overflow attacks
            if len(self.burst_buffer) + len(lines) > MAX_BURST_BUFFER_LINES:
                logger.warning(f"Burst buffer limit reached ({MAX_BURST_BUFFER_LINES} lines), "
                             f"dropping {len(lines)} new lines. Possible DoS attempt or misconfiguration.")
                self.dropped_messages += 1
                return
            
            self.burst_buffer.extend(lines)
            self.last_receive_time = time.time()
            logger.debug(f"Received {len(lines)} lines, buffer now has {len(self.burst_buffer)} lines")
    
    def _check_burst_completion(self):
        """Check if burst is complete and process if so."""
        burst_lines = None
        with self.metrics_lock:
            if not self.burst_buffer:
                return
            
            time_since_last = time.time() - self.last_receive_time
            if time_since_last < self.completion_timeout:
                return
            
            # Burst is complete, copy buffer and release lock before parsing
            burst_lines = self.burst_buffer
            self.burst_buffer = []
        
        logger.debug(f"Burst complete ({len(burst_lines)} lines), processing...")
        
        try:
            new_metrics = self.line_parser(burst_lines)
            with self.metrics_lock:
                self.cached_metrics = new_metrics
                self.parse_count += 1
            logger.info(f"Updated {len(new_metrics)} metrics from UDP burst (parse #{self.parse_count})")
        except Exception as e:
            logger.error(f"Error parsing UDP burst: {e}", exc_info=True)
    
    def get_metrics(self) -> List[Metric]:
        """Get current metrics (thread-safe)."""
        with self.metrics_lock:
            return self.cached_metrics.copy()


class ImpstatsCollector:
    """
    Prometheus collector that reads and parses impstats.
    Supports both file-based and UDP listener modes.
    """
    
    def __init__(self, mode: str, file_path: str = None, format_type: str = "json", 
                 udp_addr: str = None, udp_port: int = None, completion_timeout: float = 5):
        self.mode = mode
        self.format_type = format_type
        self.parse_count = 0
        
        if mode == "file":
            if not file_path:
                raise ValueError("file_path required for file mode")
            self.file_path = file_path
            self.last_mtime = 0
            self.cached_metrics: List[Metric] = []
            
            # Select parser
            if format_type == "json":
                self.parser = parse_json_impstats
            elif format_type == "prometheus":
                self.parser = parse_prometheus_impstats
            elif format_type == "cee":
                self.parser = parse_cee_impstats
            else:
                logger.warning(f"Unknown format: {format_type}, defaulting to json")
                self.parser = parse_json_impstats
            
            self.udp_listener = None
            
        elif mode == "udp":
            if not udp_addr or not udp_port:
                raise ValueError("udp_addr and udp_port required for UDP mode")
            
            # Parse allowed sources
            allowed_sources = []
            if ALLOWED_UDP_SOURCES:
                allowed_sources = [ip.strip() for ip in ALLOWED_UDP_SOURCES.split(',') if ip.strip()]
                logger.info(f"UDP source filtering enabled: {allowed_sources}")
            
            self.udp_listener = UdpStatsListener(udp_addr, udp_port, format_type, 
                                                completion_timeout, allowed_sources)
            self.udp_listener.start()
            self.file_path = None
            self.cached_metrics = []
        
        else:
            raise ValueError(f"Unknown mode: {mode}, must be 'file' or 'udp'")
    
    def refresh_if_needed(self):
        """Check file mtime and refresh cache if file has changed (file mode only)."""
        if self.mode != "file":
            return
        
        try:
            current_mtime = os.path.getmtime(self.file_path)
            if current_mtime != self.last_mtime:
                logger.debug(f"File modified, refreshing metrics (mtime: {current_mtime})")
                self.cached_metrics = self.parser(self.file_path)
                self.last_mtime = current_mtime
                self.parse_count += 1
                logger.info(f"Loaded {len(self.cached_metrics)} metrics from {self.file_path} (parse #{self.parse_count})")
        except FileNotFoundError:
            if self.cached_metrics:
                logger.warning(f"Impstats file disappeared: {self.file_path}")
                self.cached_metrics = []
        except Exception as e:
            logger.error(f"Error checking file mtime: {e}", exc_info=True)
    
    def get_current_metrics(self) -> List[Metric]:
        """Get current metrics based on mode."""
        if self.mode == "file":
            self.refresh_if_needed()
            return self.cached_metrics
        elif self.mode == "udp":
            return self.udp_listener.get_metrics()
        return []
    
    def get_parse_count(self) -> int:
        """Get total number of times metrics were parsed/updated."""
        if self.mode == "file":
            return self.parse_count
        elif self.mode == "udp":
            return self.udp_listener.parse_count
        return 0
    
    def collect(self):
        """
        Called by Prometheus client library on each scrape.
        Returns metric families.
        """
        current_metrics = self.get_current_metrics()
        
        # Group metrics by name
        metric_groups: Dict[str, List[Metric]] = defaultdict(list)
        for metric in current_metrics:
            metric_groups[metric.name].append(metric)
        
        # Emit metric families
        for name, metrics in metric_groups.items():
            if not metrics:
                continue
            
            # Determine type from first metric
            metric_type = metrics[0].metric_type
            
            if metric_type == "counter":
                family = CounterMetricFamily(
                    name,
                    f"rsyslog metric {name}",
                    labels=list(metrics[0].labels.keys()) if metrics[0].labels else None,
                )
            else:
                family = GaugeMetricFamily(
                    name,
                    f"rsyslog metric {name}",
                    labels=list(metrics[0].labels.keys()) if metrics[0].labels else None,
                )
            
            for metric in metrics:
                if metric.labels:
                    family.add_metric(
                        labels=list(metric.labels.values()),
                        value=metric.value,
                    )
                else:
                    family.add_metric([], metric.value)
            
            yield family


class ExporterApp:
    """WSGI application for the Prometheus exporter."""
    
    def __init__(self, collector: ImpstatsCollector):
        self.collector = collector
        self.registry = CollectorRegistry()
        self.registry.register(collector)
        self.start_time = time.time()
    
    def __call__(self, environ, start_response):
        request = Request(environ)
        
        if request.path == "/metrics":
            # Check if we have any metrics to export
            current_metrics = self.collector.get_current_metrics()
            
            if not current_metrics:
                # Best practice: return 503 with explanatory comment when no metrics available
                # This allows Prometheus to distinguish between "no data" and "service down"
                error_message = (
                    "# No metrics available\n"
                    "# The rsyslog exporter has not yet collected any statistics.\n"
                )
                
                if self.collector.mode == "file":
                    if not os.path.exists(self.collector.file_path):
                        error_message += f"# Reason: impstats file does not exist: {self.collector.file_path}\n"
                    else:
                        error_message += f"# Reason: impstats file is empty or contains no valid metrics\n"
                elif self.collector.mode == "udp":
                    error_message += "# Reason: No statistics received via UDP yet. Waiting for rsyslog to send data.\n"
                
                response = Response(
                    error_message,
                    status=503,
                    mimetype="text/plain; version=0.0.4"
                )
            else:
                # Generate Prometheus exposition format
                output = generate_latest(self.registry)
                response = Response(output, mimetype="text/plain; version=0.0.4")
        
        elif request.path == "/health" or request.path == "/":
            # Health check endpoint
            uptime = time.time() - self.start_time
            current_metrics = self.collector.get_current_metrics()
            
            # Derive simple health status
            status = "healthy"
            try:
                if self.collector.mode == "file":
                    # degraded if file missing or metrics are empty after a parse
                    if not os.path.exists(self.collector.file_path):
                        status = "degraded"
                    if len(current_metrics) == 0 and self.collector.get_parse_count() > 0:
                        status = "degraded"
                elif self.collector.mode == "udp":
                    if getattr(self.collector.udp_listener, "dropped_messages", 0) > 0:
                        status = "degraded"
            except Exception:
                status = "degraded"

            health_info = {
                "status": status,
                "uptime_seconds": round(uptime, 2),
                "mode": self.collector.mode,
                "metrics_count": len(current_metrics),
                "parse_count": self.collector.get_parse_count(),
            }
            
            if self.collector.mode == "file":
                health_info["impstats_file"] = self.collector.file_path
                health_info["impstats_format"] = self.collector.format_type
            elif self.collector.mode == "udp":
                health_info["udp_addr"] = self.collector.udp_listener.addr
                health_info["udp_port"] = self.collector.udp_listener.port
                health_info["impstats_format"] = self.collector.format_type
                health_info["dropped_messages"] = self.collector.udp_listener.dropped_messages
                if self.collector.udp_listener.allowed_sources:
                    health_info["source_filtering"] = "enabled"
            
            response = Response(
                json.dumps(health_info, indent=2) + "\n",
                mimetype="application/json",
            )
        
        else:
            response = Response("Not Found\n", status=404)
        
        return response(environ, start_response)


# Global collector instance for WSGI application
_collector = None


def create_app():
    """WSGI application factory for production servers (gunicorn, uwsgi, etc.)."""
    global _collector
    
    # Validate configuration
    if IMPSTATS_MODE not in ("file", "udp"):
        logger.error(f"Invalid IMPSTATS_MODE: {IMPSTATS_MODE}")
        sys.exit(1)
    
    if IMPSTATS_FORMAT not in ("json", "prometheus", "cee"):
        logger.error(f"Invalid IMPSTATS_FORMAT: {IMPSTATS_FORMAT}")
        sys.exit(1)
    
    if not (1 <= LISTEN_PORT <= 65535):
        logger.error(f"Invalid LISTEN_PORT: {LISTEN_PORT}")
        sys.exit(1)
    
    if IMPSTATS_MODE == "udp" and not (1 <= IMPSTATS_UDP_PORT <= 65535):
        logger.error(f"Invalid IMPSTATS_UDP_PORT: {IMPSTATS_UDP_PORT}")
        sys.exit(1)

    # Enforce single worker in UDP mode to avoid socket binding clashes with gunicorn
    if IMPSTATS_MODE == "udp":
        detected_workers = 1
        # common envs: WEB_CONCURRENCY, GUNICORN_WORKERS, WORKERS
        for env_key in ("WEB_CONCURRENCY", "GUNICORN_WORKERS", "WORKERS"):
            val = os.getenv(env_key)
            if val:
                try:
                    detected_workers = int(val)
                    break
                except ValueError:
                    pass
        if detected_workers == 1:
            # parse from GUNICORN_CMD_ARGS like "--workers 4" or "-w 4"
            cmd_args = os.getenv("GUNICORN_CMD_ARGS", "")
            m = re.search(r"--workers\s+(\d+)", cmd_args)
            if not m:
                m = re.search(r"(?:\s|^)\-w\s+(\d+)(?:\s|$)", cmd_args)
            if m:
                try:
                    detected_workers = int(m.group(1))
                except ValueError:
                    detected_workers = 1
        if detected_workers > 1 and os.getenv("RSYSLOG_EXPORTER_ALLOW_MULTI_WORKER_UDP", "0") != "1":
            logger.error(
                "UDP mode requires a single worker to avoid socket clashes. Detected workers=%d. "
                "Set --workers 1 or export RSYSLOG_EXPORTER_ALLOW_MULTI_WORKER_UDP=1 to override (not recommended).",
                detected_workers,
            )
            sys.exit(1)
    
    logger.info("=" * 60)
    logger.info("rsyslog Prometheus Exporter initializing")
    logger.info("=" * 60)
    logger.info(f"Mode: {IMPSTATS_MODE}")
    logger.info(f"Impstats format: {IMPSTATS_FORMAT}")
    logger.info(f"HTTP bind: {LISTEN_ADDR}:{LISTEN_PORT}")
    logger.info(f"Log level: {LOG_LEVEL}")
    
    if IMPSTATS_MODE == "file":
        logger.info(f"File path: {IMPSTATS_PATH}")
        
        # Validate impstats file exists
        if not os.path.exists(IMPSTATS_PATH):
            logger.warning(f"Impstats file does not exist yet: {IMPSTATS_PATH}")
            logger.warning("Exporter will start but metrics will be empty until file is created")
        
        # Create collector in file mode
        _collector = ImpstatsCollector(
            mode="file",
            file_path=IMPSTATS_PATH,
            format_type=IMPSTATS_FORMAT
        )
        
        # Do initial load
        try:
            _collector.refresh_if_needed()
            logger.info(f"Initial load: {len(_collector.cached_metrics)} metrics")
        except Exception as e:
            logger.error(f"Initial load failed: {e}")
    
    elif IMPSTATS_MODE == "udp":
        logger.info(f"UDP listen: {IMPSTATS_UDP_ADDR}:{IMPSTATS_UDP_PORT}")
        logger.info(f"Burst completion timeout: {STATS_COMPLETE_TIMEOUT}s")
        
        # Create collector in UDP mode
        _collector = ImpstatsCollector(
            mode="udp",
            format_type=IMPSTATS_FORMAT,
            udp_addr=IMPSTATS_UDP_ADDR,
            udp_port=IMPSTATS_UDP_PORT,
            completion_timeout=STATS_COMPLETE_TIMEOUT
        )
        logger.info("UDP listener started, waiting for stats from rsyslog...")
    
    else:
        logger.error(f"Invalid IMPSTATS_MODE: {IMPSTATS_MODE}, must be 'file' or 'udp'")
        sys.exit(1)
    
    # Create WSGI app
    app = ExporterApp(_collector)
    logger.info("=" * 60)
    logger.info("Application initialized successfully")
    logger.info("=" * 60)
    
    return app


def main():
    """Development server entry point (uses Werkzeug - not for production!)."""
    logger.warning("*" * 60)
    logger.warning("DEVELOPMENT MODE - Not suitable for production!")
    logger.warning("For production, use: gunicorn --workers 1 -b %s:%d rsyslog_exporter:application", LISTEN_ADDR, LISTEN_PORT)
    logger.warning("*" * 60)
    
    logger.info(f"Starting development server at http://{LISTEN_ADDR}:{LISTEN_PORT}/metrics")
    
    try:
        run_simple(
            LISTEN_ADDR,
            LISTEN_PORT,
            application,
            use_reloader=False,
            use_debugger=False,
            threaded=True,
        )
    except KeyboardInterrupt:
        logger.info("Received shutdown signal, exiting")
    except Exception as e:
        logger.error(f"Server error: {e}", exc_info=True)
        sys.exit(1)


# WSGI application instance for production servers (gunicorn, uwsgi, etc.)
# Created at module import time so it's available for both gunicorn and direct execution
application = create_app()

if __name__ == "__main__":
    main()
