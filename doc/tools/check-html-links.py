#!/usr/bin/env python3
"""Deterministic checker for local links in generated HTML docs.

Usage:
  ./doc/tools/check-html-links.py [build_dir]

By default it scans doc/build under the repository root.
"""

from __future__ import annotations

import argparse
import os
import sys
from dataclasses import dataclass
from html.parser import HTMLParser
from pathlib import Path
from typing import Iterable
from urllib.parse import unquote, urljoin, urlsplit

SKIP_SCHEMES = {"mailto", "javascript", "tel", "data"}
LOCAL_HOST = "local.docs"


@dataclass(frozen=True)
class LinkRef:
    source: Path
    url: str
    base_href: str | None


@dataclass(frozen=True)
class Issue:
    source: Path
    url: str
    resolved: str
    reason: str


class _DocHTMLParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.links: set[str] = set()
        self.ids: set[str] = set()
        self.base_href: str | None = None

    def _handle_attrs(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attr_map = {k.lower(): v for k, v in attrs if k and v is not None}

        if tag == "base" and self.base_href is None:
            base = (attr_map.get("href") or "").strip()
            if base:
                self.base_href = base

        for key in ("href", "src"):
            value = (attr_map.get(key) or "").strip()
            if value:
                self.links.add(value)

        srcset = (attr_map.get("srcset") or "").strip()
        if srcset:
            for part in srcset.split(","):
                candidate = part.strip().split()[0] if part.strip() else ""
                if candidate:
                    self.links.add(candidate)

        id_value = (attr_map.get("id") or "").strip()
        if id_value:
            self.ids.add(id_value)

        name_value = (attr_map.get("name") or "").strip()
        if name_value:
            self.ids.add(name_value)

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._handle_attrs(tag.lower(), attrs)

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self._handle_attrs(tag.lower(), attrs)


def iter_html_files(root: Path) -> Iterable[Path]:
    for path in sorted(root.rglob("*.html")):
        if path.is_file():
            yield path


def parse_page(path: Path) -> _DocHTMLParser:
    parser = _DocHTMLParser()
    text = path.read_text(encoding="utf-8", errors="replace")
    parser.feed(text)
    parser.close()
    return parser


def parse_links(path: Path) -> list[LinkRef]:
    parsed = parse_page(path)
    return [
        LinkRef(source=path, url=url, base_href=parsed.base_href)
        for url in sorted(parsed.links)
    ]


def _build_page_url(source: Path, build_root: Path, base_href: str | None) -> str:
    rel = source.relative_to(build_root).as_posix()
    page_url = f"https://{LOCAL_HOST}/{rel}"
    if not base_href:
        return page_url
    return urljoin(page_url, base_href)


def normalize_target(
    source: Path,
    raw_url: str,
    base_href: str | None,
    build_root: Path,
    check_root_absolute: bool,
) -> tuple[Path | None, str | None, str | None]:
    if raw_url.startswith("/") and not check_root_absolute:
        return None, None, None

    effective_page_url = _build_page_url(source, build_root, base_href)
    resolved_url = urljoin(effective_page_url, raw_url)
    parsed = urlsplit(resolved_url)

    if parsed.scheme and parsed.scheme.lower() in SKIP_SCHEMES:
        return None, None, None

    if parsed.netloc and parsed.netloc != LOCAL_HOST:
        return None, None, None

    url_path = unquote(parsed.path)
    fragment = parsed.fragment or None

    if url_path.startswith("/"):
        candidate = build_root / url_path.lstrip("/")
    elif url_path:
        candidate = source.parent / url_path
    else:
        candidate = source

    candidate = candidate.resolve(strict=False)
    if candidate.is_dir():
        candidate = candidate / "index.html"

    build_root_resolved = build_root.resolve(strict=False)
    if not candidate.is_relative_to(build_root_resolved):
        return candidate, fragment, "escaped-build-root"

    return candidate, fragment, None


def load_ids(path: Path, cache: dict[Path, set[str]]) -> set[str]:
    if path in cache:
        return cache[path]
    parsed = parse_page(path)
    cache[path] = parsed.ids
    return cache[path]


def check_links(build_root: Path, check_anchors: bool, check_root_absolute: bool) -> list[Issue]:
    issues: list[Issue] = []
    id_cache: dict[Path, set[str]] = {}
    exists_cache: dict[Path, bool] = {}

    for html_file in iter_html_files(build_root):
        for ref in parse_links(html_file):
            target, fragment, reason = normalize_target(
                ref.source,
                ref.url,
                ref.base_href,
                build_root,
                check_root_absolute,
            )
            if target is None:
                continue
            if reason is not None:
                issues.append(
                    Issue(
                        source=ref.source,
                        url=ref.url,
                        resolved=str(target),
                        reason=reason,
                    )
                )
                continue

            exists = exists_cache.get(target)
            if exists is None:
                exists = target.exists()
                exists_cache[target] = exists

            if not exists:
                issues.append(
                    Issue(
                        source=ref.source,
                        url=ref.url,
                        resolved=str(target),
                        reason="missing-target",
                    )
                )
                continue

            if check_anchors and fragment:
                decoded = unquote(fragment)
                ids = load_ids(target, id_cache)
                if decoded not in ids:
                    issues.append(
                        Issue(
                            source=ref.source,
                            url=ref.url,
                            resolved=f"{target}#{decoded}",
                            reason="missing-anchor",
                        )
                    )

    issues.sort(key=lambda i: (str(i.source), i.url, i.reason, i.resolved))
    return issues


def default_build_dir() -> Path:
    # script: repo/doc/tools/check-html-links.py -> repo/doc/build
    return Path(__file__).resolve().parents[1] / "build"


def main() -> int:
    parser = argparse.ArgumentParser(description="Check generated HTML for invalid local links.")
    parser.add_argument("build_dir", nargs="?", default=str(default_build_dir()),
                        help="Path to generated HTML root (default: doc/build)")
    parser.add_argument("--check-anchors", action="store_true", help="Also verify #fragment anchors")
    parser.add_argument(
        "--check-root-absolute",
        action="store_true",
        help="Validate href values starting with '/' against build root",
    )
    args = parser.parse_args()

    build_root = Path(args.build_dir).resolve()
    if not build_root.exists():
        print(f"ERROR: build directory does not exist: {build_root}", file=sys.stderr)
        return 2

    issues = check_links(build_root, check_anchors=args.check_anchors, check_root_absolute=args.check_root_absolute)

    if not issues:
        print("OK: no invalid local links found")
        return 0

    print(f"FAIL: found {len(issues)} invalid local links")
    for issue in issues:
        src = os.path.relpath(issue.source, build_root)
        print(f"{src}\t{issue.reason}\t{issue.url}\t{issue.resolved}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
