#!/usr/bin/env python3
"""Decode the reviewable hexadecimal imtcp seed corpus for libFuzzer."""

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()

    args.destination.mkdir(parents=True, exist_ok=True)
    for source in sorted(args.source.glob("*.hex")):
        encoded = "".join(source.read_text(encoding="ascii").split())
        (args.destination / source.stem).write_bytes(bytes.fromhex(encoded))


if __name__ == "__main__":
    main()
