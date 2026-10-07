#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Reserve two loopback TCP ports until a test explicitly releases them."""

import socket
import sys


def main():
    reservations = []
    try:
        for _ in range(2):
            listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            reservations.append(listener)
            listener.bind(("127.0.0.1", 0))
        ports = [listener.getsockname()[1] for listener in reservations]
        print(f"READY {ports[0]} {ports[1]}", flush=True)

        for command in sys.stdin:
            command = command.strip()
            if command in ("release 1", "release 2"):
                index = int(command[-1]) - 1
                if reservations[index] is None:
                    raise RuntimeError(f"reservation {index + 1} was already released")
                reservations[index].close()
                reservations[index] = None
                print(f"RELEASED {index + 1}", flush=True)
            elif command == "release all":
                for index, listener in enumerate(reservations):
                    if listener is not None:
                        listener.close()
                        reservations[index] = None
                print("RELEASED all", flush=True)
                return
            else:
                raise RuntimeError(f"invalid command: {command}")
        raise RuntimeError("control input closed before release all")
    except (OSError, RuntimeError) as error:
        print(f"ERROR {error}", flush=True)
        raise
    finally:
        for listener in reservations:
            if listener is not None:
                listener.close()


if __name__ == "__main__":
    main()
