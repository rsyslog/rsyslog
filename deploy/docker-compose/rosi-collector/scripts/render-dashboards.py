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
"""Render Grafana dashboards from templates with shared navigation/links."""
from __future__ import annotations

import json
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    if not path.exists():
        print(f"Error: missing file {path}", file=sys.stderr)
        sys.exit(1)
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"Error: invalid JSON in {path}: {e}", file=sys.stderr)
        sys.exit(1)
    except OSError as e:
        print(f"Error: cannot read {path}: {e}", file=sys.stderr)
        sys.exit(1)


def _write_json(path: Path, data: dict) -> None:
    try:
        path.write_text(
            json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
    except (TypeError, ValueError) as e:
        print(f"Error: cannot serialize JSON for {path}: {e}", file=sys.stderr)
        sys.exit(1)
    except OSError as e:
        print(f"Error: cannot write {path}: {e}", file=sys.stderr)
        sys.exit(1)


def _merge_links(shared_links: list[dict], template_links: list[dict]) -> list[dict]:
    seen = {(link.get("title"), link.get("url")) for link in shared_links}
    extras = [
        link
        for link in template_links
        if (link.get("title"), link.get("url")) not in seen
    ]
    return [*shared_links, *extras]


def _is_nav_panel(panel: dict, nav_title: str) -> bool:
    return panel.get("type") == "text" and panel.get("title") == nav_title


def _shift_panels_y(panels: list[dict], delta_y: int) -> None:
    """Shift gridPos.y by delta_y for all panels and nested panels (e.g. inside rows)."""
    for panel in panels:
        if "gridPos" in panel and isinstance(panel["gridPos"], dict):
            y = panel["gridPos"].get("y", 0)
            panel["gridPos"]["y"] = y + delta_y
        nested = panel.get("panels")
        if isinstance(nested, list):
            _shift_panels_y(nested, delta_y)


def main() -> None:
    base_dir = Path(__file__).resolve().parents[1] / "grafana" / "provisioning" / "dashboards"
    templates_dir = base_dir / "templates"
    output_dir = base_dir / "generated"
    shared_path = templates_dir / "_shared.json"

    shared = _load_json(shared_path)
    shared_links = shared.get("links", [])
    nav_title = "Dashboards"

    try:
        output_dir.mkdir(parents=True, exist_ok=True)
    except OSError as e:
        print(f"Error: cannot create output directory {output_dir}: {e}", file=sys.stderr)
        sys.exit(1)

    for template_path in templates_dir.glob("*.json"):
        if template_path.name == "_shared.json":
            continue

        dashboard = _load_json(template_path)
        dashboard["links"] = _merge_links(shared_links, dashboard.get("links", []))

        panels = [panel for panel in dashboard.get("panels", []) if not _is_nav_panel(panel, nav_title)]
        min_y = min((panel.get("gridPos", {}).get("y", 0) for panel in panels), default=0)
        if min_y > 0:
            _shift_panels_y(panels, -min_y)

        dashboard["panels"] = panels

        output_path = output_dir / template_path.name
        _write_json(output_path, dashboard)


if __name__ == "__main__":
    main()
