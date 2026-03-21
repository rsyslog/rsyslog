#!/usr/bin/env python3
"""Sync Docker Hub repository descriptions for the rsyslog image family.

By default the script runs in dry-run mode and prints the intended updates.
Use --apply to write metadata to Docker Hub.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request


HUB_LOGIN_URL = "https://hub.docker.com/v2/users/login/"
HUB_REPO_URL = "https://hub.docker.com/v2/repositories/{namespace}/{repo}/"
DEFAULT_NAMESPACE = "rsyslog"
DEFAULT_METADATA_FILE = Path(__file__).with_name("dockerhub_metadata.json")


def load_metadata(path: Path) -> dict[str, dict[str, str]]:
    return json.loads(path.read_text())


def load_credentials() -> tuple[str, str]:
    username = os.getenv("DOCKERHUB_USERNAME")
    password = os.getenv("DOCKERHUB_PASSWORD")
    if username and password:
        return username, password

    cfg_path = Path.home() / ".docker" / "config.json"
    if not cfg_path.exists():
        raise RuntimeError(
            "No Docker Hub credentials found. Set DOCKERHUB_USERNAME and "
            "DOCKERHUB_PASSWORD or login with docker first."
        )

    cfg = json.loads(cfg_path.read_text())
    auths = cfg.get("auths", {})
    auth = auths.get("https://index.docker.io/v1/", {}).get("auth")
    if not auth:
        raise RuntimeError(
            "Docker config does not contain https://index.docker.io/v1/ auth."
        )

    try:
        decoded = base64.b64decode(auth).decode()
    except (binascii.Error, UnicodeDecodeError) as err:
        raise RuntimeError("Docker config auth for Docker Hub is malformed.") from err

    if ":" not in decoded:
        raise RuntimeError("Docker config auth for Docker Hub is malformed.")

    return tuple(decoded.split(":", 1))


def hub_request(
    url: str, token: str | None = None, method: str = "GET", payload: dict | None = None
) -> dict:
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", f"JWT {token}")
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode())


def login() -> str:
    username, password = load_credentials()
    resp = hub_request(HUB_LOGIN_URL, method="POST", payload={"username": username, "password": password})
    token = resp.get("token")
    if not token:
        raise RuntimeError("Docker Hub login succeeded but no token was returned.")
    return token


def normalize_repo_selection(selected: list[str] | None, metadata: dict[str, dict[str, str]]) -> list[str]:
    if not selected:
        return sorted(metadata.keys())
    missing = [name for name in selected if name not in metadata]
    if missing:
        raise RuntimeError(f"Metadata file does not define repos: {', '.join(missing)}")
    return selected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="Write metadata to Docker Hub.")
    parser.add_argument("--namespace", default=DEFAULT_NAMESPACE, help="Docker Hub namespace. Default: rsyslog")
    parser.add_argument(
        "--metadata-file",
        default=str(DEFAULT_METADATA_FILE),
        help="Path to the JSON metadata file.",
    )
    parser.add_argument(
        "--repo",
        action="append",
        help="Limit sync to one or more repos defined in the metadata file.",
    )
    args = parser.parse_args()

    metadata_file = Path(args.metadata_file)
    metadata = load_metadata(metadata_file)
    repos = normalize_repo_selection(args.repo, metadata)

    token = login()

    for repo in repos:
        payload = metadata[repo]
        url = HUB_REPO_URL.format(namespace=args.namespace, repo=repo)
        current = hub_request(url, token=token)
        summary = {
            "repo": f"{args.namespace}/{repo}",
            "current_description": current.get("description") or "",
            "new_description": payload["description"],
            "apply": args.apply,
        }
        print(json.dumps(summary, ensure_ascii=True))
        if args.apply:
            updated = hub_request(url, token=token, method="PATCH", payload=payload)
            print(
                json.dumps(
                    {
                        "repo": f"{args.namespace}/{repo}",
                        "updated_description": updated.get("description") or "",
                        "has_full_description": updated.get("full_description") is not None,
                    },
                    ensure_ascii=True,
                )
            )

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except urllib.error.HTTPError as err:
        print(f"HTTP error from Docker Hub: {err.code} {err.reason}", file=sys.stderr)
        raise
    except Exception as err:  # pragma: no cover - CLI path
        print(f"ERROR: {err}", file=sys.stderr)
        raise SystemExit(1)
