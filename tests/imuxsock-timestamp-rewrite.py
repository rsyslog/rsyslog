#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Send deterministic datagrams and check the reported rewrite boundary."""
import collections
import pathlib
import socket
import sys


def cases():
    # Cross the inline raw-buffer and heap-allocation boundaries. The all-zero
    # PRI avoids arithmetic overflow obscuring the original bounds violation.
    for count in (84, 90, 91, 100):
        for stamp in ("Jan 1 ::", "0-1-1T::Z"):
            yield "<" + "0" * count + ">" + stamp, None
    for raw in ("<", "<>", "<999>Jan 1 ::", "<0000>Jan 1 ::", "1234567890",
                "no-pri", "<1>Jan 1 ::",
                "<1>0-1-1T::Z", "<1>Jan  1 00:00:00"):
        yield raw, None
    for ending in ("\0", "\n", "\r"):
        yield "<" + "0" * 100 + ">Jan 1 ::" + ending, None
    # This is the synchronized final control for every listener.
    yield "<13>Jan  1 00:00:00 sentinel: done", ("Jan  1 00:00:00", "sentinel:", " done")


def main():
    operation, prefix = sys.argv[1:3]
    modes = ("default", "trusted", "annotated", "nosystime", "chain")
    inputs = list(cases())
    assert len(inputs) == 21
    if operation == "send":
        for mode in modes:
            with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as client:
                client.connect(prefix + "-" + mode + ".sock")
                for raw, _ in inputs:
                    client.send(raw.encode("ascii"))
        return
    lines = pathlib.Path(sys.argv[3]).read_text().split("\n")[:-1]
    assert len(lines) == len(inputs) * len(modes), (len(lines), len(inputs) * len(modes))
    # Listener scheduling is intentionally unordered. Check multiplicities,
    # rather than assuming arrival order across independent input sockets.
    raws = collections.Counter(line.split("|", 1)[0] for line in lines)
    for raw, fields in inputs:
        if fields is None and not raw.endswith(("\0", "\n", "\r")):
            assert raws[raw] >= 4, ("raw message changed", repr(raw), raws[raw])
    controls = [line for line in lines if "sentinel: done" in line]
    for line in controls:
        raw, tag, body = line.split("|", 2)
        if " @[" in raw:
            continue  # Trusted text annotation intentionally extends the body.
        assert (tag, body) == ("sentinel:", " done"), line
    # At least the two system-timestamp special-parser listeners must rewrite
    # the ordinary full-width controls to the current receive timestamp.
    original = "<13>Jan  1 00:00:00 sentinel: done"
    assert raws[original] == 2, ("full-width timestamp was not rewritten", original, raws[original])
    print("checked", len(lines), "real-daemon datagrams across", len(modes), "listener modes")


if __name__ == "__main__":
    main()
