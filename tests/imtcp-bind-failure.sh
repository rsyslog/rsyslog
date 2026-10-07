#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# A held loopback port must reject startup only when both failOnBindError and
# abortOnUncleanConfig are on. A ten-second exit limit catches startup hangs;
# surviving a one-second wait proves the compatibility cases stayed running.
# Both RainerScript and YAML pass through the same module parameter backend.
# Diagnostics are asserted from stderr because activation fails before an input
# can deliver the message to a configured rsyslog output.
. ${srcdir:=.}/diag.sh init
require_plugin imtcp

python3 - <<'PY' || error_exit 1
import os
import itertools
import pathlib
import socket
import subprocess
import tempfile

root = pathlib.Path.cwd().parent
daemon = root / "tools/rsyslogd"
modules = ":".join(str(root / path) for path in
                   ("plugins/imtcp/.libs", "plugins/omfile/.libs", "runtime/.libs"))
holder = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
holder.bind(("127.0.0.1", 0))
holder.listen(1)
port = holder.getsockname()[1]

with tempfile.TemporaryDirectory(prefix="imtcp-bind-") as tmp:
    yaml_enabled = "#define HAVE_LIBYAML 1" in (root / "config.h").read_text(encoding="utf-8")
    formats = ("rsyslog", "yaml") if yaml_enabled else ("rsyslog",)
    cases = ((False, True, False), (True, False, False), (True, True, True))
    for format_name, (fail_on_bind, abort_unclean, should_exit) in itertools.product(formats, cases):
        config = pathlib.Path(tmp) / f"rsyslog.{format_name}"
        abort_value = "on" if abort_unclean else "off"
        fail_value = "on" if fail_on_bind else "off"
        if format_name == "yaml":
            contents = (f'version: 2\n'
                        f'global:\n  abortOnUncleanConfig: "{abort_value}"\n'
                        f'modules:\n  - load: imtcp\n    failOnBindError: "{fail_value}"\n'
                        f'inputs:\n  - type: imtcp\n    address: "127.0.0.1"\n    port: "{port}"\n'
                        f'rulesets:\n  - name: main\n    script: |\n'
                        f'      action(type="omfile" file="{tmp}/out.log")\n')
        else:
            contents = (f'global(abortOnUncleanConfig="{abort_value}")\n'
                        f'module(load="imtcp" failOnBindError="{fail_value}")\n'
                        f'input(type="imtcp" address="127.0.0.1" port="{port}")\n'
                        f'action(type="omfile" file="{tmp}/out.log")\n')
        config.write_text(contents, encoding="utf-8")
        process = subprocess.Popen((str(daemon), "-n", "-iNONE", "-f", str(config),
                                    "-M", modules), stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True)
        try:
            if should_exit:
                status = process.wait(timeout=10)
                assert status != 0, "strict listener failure exited successfully"
            else:
                try:
                    process.wait(timeout=1)
                    raise AssertionError("compatibility startup exited unexpectedly")
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
        output = process.stdout.read()
        assert "Could not create tcp listener" in output, output
        if should_exit:
            assert "activation of module imtcp failed" in output, output
        if "one visible listener is PID" in output:
            assert f"PID {os.getpid()}" in output, output
        else:
            assert "listener owner unavailable" in output, output

holder.close()
PY
exit_test
