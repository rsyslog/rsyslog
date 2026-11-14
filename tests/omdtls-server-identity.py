#!/usr/bin/env python3
"""Loopback DTLS controls for omdtls server identity verification."""
import pathlib
import os
import re
import select
import sys
import socket
import subprocess
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD = pathlib.Path.cwd().parent if (pathlib.Path.cwd().parent / "tools/rsyslogd").exists() else ROOT
CERTS = ROOT / "tests/testsuites/x.509"
CERT = CERTS / "client-cert.pem"
KEY = CERTS / "client-key.pem"
RSYSLOG = BUILD / "tools/rsyslogd"


YAML_MODE = "--yaml" in sys.argv


def run_case(label, mode, peer, should_connect, config_error=False, cert=CERT, key=KEY,
             recover_after_config_error=False):
    server = subprocess.Popen(["openssl", "s_server", "-dtls", "-accept", "0",
                               "-cert", str(cert), "-key", str(key), "-state"],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        startup_output = b""
        ready_deadline = time.monotonic() + 5
        match = None
        while time.monotonic() < ready_deadline and server.poll() is None:
            if select.select([server.stdout], [], [], 0.2)[0]:
                startup_output += os.read(server.stdout.fileno(), 4096)
                match = re.search(rb"ACCEPT .*:(\d+)\r?\n", startup_output)
                if match:
                    break
        assert match, f"{label}: OpenSSL DTLS listener did not bind: {startup_output[-500:]}"
        return run_case_with_server(label, mode, peer, should_connect, config_error,
                                    cert, key, recover_after_config_error, server, int(match.group(1)))
    finally:
        server.terminate()
        try:
            server.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.communicate()


def run_case_with_server(label, mode, peer, should_connect, config_error, cert, key,
                         recover_after_config_error, server, server_port):
    with tempfile.TemporaryDirectory(prefix="omdtls-identity-") as directory:
        tmp = pathlib.Path(directory)
        input_port_file = tmp / "imudp.port"
        marker = tmp / "proper-termination"
        peer_setting = f' tls.permittedpeer="{peer}"' if peer is not None else ""
        cfg = tmp / ("rsyslog.yaml" if YAML_MODE else "rsyslog.conf")
        if YAML_MODE:
            template = (ROOT / "tests/testsuites/yaml-omdtls-server-identity.yaml").read_text()
            for placeholder, value in {
                "@IMUDP@": str(BUILD / "plugins/imudp/.libs/imudp.so"),
                "@OMDTLS@": str(BUILD / "plugins/omdtls/.libs/omdtls.so"),
                "@IMDIAG@": str(BUILD / "plugins/imdiag/.libs/imdiag.so"),
                "@IMDIAG_PORT@": str(tmp / "imdiag.port"), "@MARKER@": str(marker),
                "@CA@": str(CERTS / "ca.pem"), "@CLIENT_CERT@": str(CERT), "@CLIENT_KEY@": str(KEY),
                "@INPUT_PORT_FILE@": str(input_port_file), "@SERVER_PORT@": str(server_port),
                "@MODE@": mode,
                "# @PERMITTED_PEER@": f'        tls.permittedpeer: "{peer}"' if peer is not None else "",
            }.items():
                template = template.replace(placeholder, value)
            cfg.write_text(template)
        else:
            cfg.write_text(f'''
module(load="{BUILD / 'plugins/imdiag/.libs/imdiag.so'}"
       serverrun="0" listenportfilename="{tmp / 'imdiag.port'}"
       aborttimeout="30" properterminationfile="{marker}")
global(defaultNetstreamDriverCAFile="{CERTS / 'ca.pem'}"
       defaultNetstreamDriverCertFile="{CERT}"
       defaultNetstreamDriverKeyFile="{KEY}")
module(load="{BUILD / 'plugins/imudp/.libs/imudp.so'}")
module(load="{BUILD / 'plugins/omdtls/.libs/omdtls.so'}")
input(type="imudp" address="127.0.0.1" port="0" listenPortFileName="{input_port_file}")
template(name="raw" type="string" string="%msg%")
*.* action(type="omdtls" target="127.0.0.1" port="{server_port}"
             tls.authmode="{mode}"{peer_setting} template="raw"
             action.resumeRetryCount="0")
''')
            if recover_after_config_error:
                with cfg.open("a") as stream:
                    stream.write(f'''\nglobal(abortOnUncleanConfig="off")
*.* action(type="omdtls" target="127.0.0.1" port="{server_port}"
             tls.authmode="certvalid" template="raw" action.resumeRetryCount="0")
''')
        check = subprocess.run([str(RSYSLOG), "-M", str(BUILD / "runtime/.libs"),
                                "-N1", "-f", str(cfg)], capture_output=True, text=True, timeout=20)
        if config_error:
            assert check.returncode != 0, f"{label}: malformed pin was accepted"
            assert "fingerprint authentication requires" in check.stdout + check.stderr, (
                f"{label}: unexpected configuration failure: {check.stdout} {check.stderr}")
            if not recover_after_config_error:
                return
        if not config_error:
            assert check.returncode == 0, f"{label}: config failed: {check.stdout} {check.stderr}"

        daemon = None
        shutdown_timeout = False
        try:
            daemon = subprocess.Popen([str(RSYSLOG), "-d", "-M", str(BUILD / "runtime/.libs"),
                                       "-n", "-i", str(tmp / "rsyslog.pid"), "-f", str(cfg)],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                      env={**os.environ, "RSYSLOG_DEBUG": "debug nostdout",
                                           "RSYSLOG_DEBUGLOG": str(tmp / "debug.log")})
            deadline = time.monotonic() + 15  # bounded CI readiness, not a presumed startup delay
            observed = False
            input_port = None
            while time.monotonic() < deadline and daemon.poll() is None:
                if input_port_file.exists():
                    contents = input_port_file.read_text().strip()
                    if contents.isdecimal() and 0 < int(contents) <= 65535:
                        input_port = int(contents)
                        break
                time.sleep(0.05)
            assert input_port is not None, f"{label}: imudp listener did not publish a valid bound port"
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                while time.monotonic() < deadline:
                    if daemon.poll() is not None:
                        break
                    sender.sendto(b"<13>Nov 14 20:00:00 localhost test: OMDTLS-IDENTITY-CANARY",
                                  ("127.0.0.1", input_port))
                    debug = (tmp / "debug.log").read_text(errors="replace") if (tmp / "debug.log").exists() else ""
                    if should_connect:
                        observed = "Exit with 0 for 127.0.0.1:" in debug and "Successfully send message" in debug
                    else:
                        observed = f"server certificate failed {mode} identity verification" in debug
                    if observed:
                        break
                    time.sleep(0.1)
        finally:
            if daemon is not None:
                daemon.terminate()
                try:
                    _, daemon_error = daemon.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    shutdown_timeout = True
                    daemon.kill()
                    _, daemon_error = daemon.communicate()
        debug = (tmp / "debug.log").read_text(errors="replace")
        connected = "dtls_connect[" in debug and "Exit with 0 for 127.0.0.1:" in debug
        sent = "Successfully send message" in debug
        relevant = [line for line in debug.splitlines() if any(term in line for term in
                    ("omdtls", "imudp", "yaml", "error", "action", "ruleset"))]
        marker_text = marker.read_text(errors="replace") if marker.exists() else ""
        clean_exit = (not shutdown_timeout and daemon.returncode == 0 and
                      "status=ok" in marker_text.splitlines() and
                      "phase=main-return" in marker_text.splitlines() and
                      f"pid={daemon.pid}" in marker_text.splitlines())
        assert clean_exit and observed and connected == should_connect and sent == should_connect, (
            f"{label}: observed={observed}, connected={connected}, sent={sent}, expected={should_connect}; "
            f"returncode={daemon.returncode}, shutdown_timeout={shutdown_timeout}, marker={marker_text!r}; "
            f"stderr={daemon_error.decode(errors='replace')[-500:]}; "
            f"debug_head={relevant[:35]}; debug_tail={relevant[-20:]}")


pin = subprocess.check_output(["openssl", "x509", "-in", str(CERT), "-noout",
                               "-fingerprint", "-sha256"], text=True).strip().split("=", 1)[1]
pin = "SHA256:" + pin
run_case("name-wrong-ip", "name", None, False)
run_case("name-explicit-cn", "name", "rsyslog client", True)
run_case("fingerprint-match", "fingerprint", pin, True)
run_case("fingerprint-mismatch", "fingerprint", pin[:-2] + "00", False)
run_case("fingerprint-short", "fingerprint", "SHA256:DE:1F", False, config_error=True)
run_case("fingerprint-missing", "fingerprint", None, False, config_error=True)
if not YAML_MODE:
    run_case("rejected-instance-followed-by-valid", "fingerprint", "SHA256:DE:1F", True,
             config_error=True, recover_after_config_error=True)
run_case("certvalid-wrong-name", "certvalid", None, True)

with tempfile.TemporaryDirectory(prefix="omdtls-san-") as directory:
    tmp = pathlib.Path(directory)
    key, csr, cert, extensions = (tmp / name for name in ("leaf.key", "leaf.csr", "leaf.pem", "leaf.ext"))
    subprocess.run(["openssl", "req", "-new", "-newkey", "rsa:2048", "-nodes",
                    "-keyout", str(key), "-out", str(csr), "-subj", "/CN=expected.test"],
                   check=True, capture_output=True)
    extensions.write_text("subjectAltName=DNS:other.test,IP:127.0.0.1\n")
    subprocess.run(["openssl", "x509", "-req", "-in", str(csr), "-CA", str(CERTS / "ca.pem"),
                    "-CAkey", str(CERTS / "ca-key.pem"), "-set_serial", "0x85F1",
                    "-out", str(cert), "-days", "1", "-extfile", str(extensions)],
                   check=True, capture_output=True)
    run_case("name-san-over-cn", "name", "expected.test", False, cert=cert, key=key)
    run_case("name-matching-san", "name", "other.test", True, cert=cert, key=key)
    run_case("name-matching-ip-san", "name", None, True, cert=cert, key=key)
    run_case("name-wrong-ip-san", "name", "127.0.0.2", False, cert=cert, key=key)
print("omdtls server identity controls passed (YAML)" if YAML_MODE else "omdtls server identity controls passed")
