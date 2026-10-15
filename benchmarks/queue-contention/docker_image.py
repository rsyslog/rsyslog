#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Resolve a Docker image ID, pulling the configured reference if needed."""

import subprocess


def inspect_or_pull_image(image, timeout_for_call):
    """Use the caller's timeout policy for each inspect and pull command."""
    command = ['docker', 'image', 'inspect', '--format', '{{.Id}}', image]
    inspected = subprocess.run(command, capture_output=True, text=True, check=False,
                               timeout=timeout_for_call())
    if inspected.returncode != 0:
        subprocess.run(['docker', 'pull', image], check=True, timeout=timeout_for_call())
        inspected = subprocess.run(command, capture_output=True, text=True, check=True,
                                   timeout=timeout_for_call())
    return inspected.stdout.strip()
