#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Send deterministic datagrams and check real-daemon raw/tag/body output."""
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
    for stamp in ("Jan 1 ::", "Jan 1 1:2:3", "Jan  1 00:00:00",
                  "2020-01-01T00:00:00.123456+01:00", "0-1-1T::Z"):
        yield "<13>" + stamp + " guard: preserved-body", (stamp, "guard:", " preserved-body")
    yield "<13>Jan 1 :: " + "A" * 100, ("Jan 1 ::", "A" * 100, "")
    for ending in ("\0", "\n", "\r"):
        yield "<" + "0" * 100 + ">Jan 1 ::" + ending, None
    # This is the synchronized final control for every listener.
    yield "<13>Jan  1 00:00:00 sentinel: done", ("Jan  1 00:00:00", "sentinel:", " done")


def main():
    operation, prefix = sys.argv[1:3]
    modes = ("default", "trusted", "annotated", "nosystime", "chain")
    inputs = list(cases())
    assert len(inputs) == 27
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
    # Short timestamp spans must not overwrite the tag even when spare bytes
    # exist in the same allocation. Long RFC3339 must use the returned cursor.
    for stamp in ("Jan 1 ::", "Jan 1 1:2:3", "0-1-1T::Z"):
        raw = "<13>" + stamp + " guard: preserved-body"
        assert raws[raw] >= 4, ("short timestamp overwrote suffix", raw, raws[raw])
    long_stamp = "<13>2020-01-01T00:00:00.123456+01:00 guard: preserved-body"
    assert raws[long_stamp] >= 4, ("long timestamp partially rewritten", raws[long_stamp])
    long_tag_raw = "<13>Jan 1 :: " + "A" * 100
    assert raws[long_tag_raw] >= 4, ("long TAG raw message changed", raws[long_tag_raw])
    long_tag_lines = [line for line in lines if line.startswith(long_tag_raw + "|")]
    assert sum(line.split("|", 2)[1:] == ["A" * 100, ""]
               for line in long_tag_lines) >= 2, long_tag_lines
    controls = [line for line in lines if "guard: preserved-body" in line or "sentinel: done" in line]
    for line in controls:
        raw, tag, body = line.split("|", 2)
        if " @[" in raw:
            continue  # Trusted text annotation intentionally extends the body.
        if "guard: preserved-body" in raw:
            assert (tag, body) == ("guard:", " preserved-body"), line
        else:
            assert (tag, body) == ("sentinel:", " done"), line
    # At least the two system-timestamp special-parser listeners must rewrite
    # the ordinary full-width controls to the current receive timestamp.
    for suffix in ("guard: preserved-body", "sentinel: done"):
        original = "<13>Jan  1 00:00:00 " + suffix
        assert raws[original] == 2, ("full-width timestamp was not rewritten", original, raws[original])
    print("checked", len(lines), "real-daemon datagrams across", len(modes), "listener modes")


if __name__ == "__main__":
    main()
