#!/usr/bin/env python3
"""Generate best-effort distro package ownership data for rsyslog modules.

The output is support data for security advisories. It answers the practical
question "which public distro package ships this upstream module file?" by
querying deterministic public package metadata.
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
import subprocess
import tarfile
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import defusedxml.ElementTree as ET


HTTP_TIMEOUT = 45
USER_AGENT = "rsyslog-distro-component-map/1.0"


@dataclass(frozen=True)
class Component:
    name: str
    upstream_path: str
    module_file: str
    kind: str


@dataclass(frozen=True)
class Source:
    key: str
    distro: str
    release: str
    channel: str
    kind: str
    urls: tuple[str, ...]


DEB_CONTENT_SOURCES = (
    Source(
        key="debian-bookworm",
        distro="debian",
        release="bookworm",
        channel="main",
        kind="deb_contents",
        urls=("https://deb.debian.org/debian/dists/bookworm/main/Contents-amd64.gz",),
    ),
    Source(
        key="debian-trixie",
        distro="debian",
        release="trixie",
        channel="main",
        kind="deb_contents",
        urls=("https://deb.debian.org/debian/dists/trixie/main/Contents-amd64.gz",),
    ),
    Source(
        key="debian-sid",
        distro="debian",
        release="sid",
        channel="main",
        kind="deb_contents",
        urls=("https://deb.debian.org/debian/dists/sid/main/Contents-amd64.gz",),
    ),
    Source(
        key="ubuntu-focal",
        distro="ubuntu",
        release="focal",
        channel="main+universe",
        kind="deb_contents",
        urls=("https://archive.ubuntu.com/ubuntu/dists/focal/Contents-amd64.gz",),
    ),
    Source(
        key="ubuntu-jammy",
        distro="ubuntu",
        release="jammy",
        channel="main+universe",
        kind="deb_contents",
        urls=("https://archive.ubuntu.com/ubuntu/dists/jammy/Contents-amd64.gz",),
    ),
    Source(
        key="ubuntu-noble",
        distro="ubuntu",
        release="noble",
        channel="main+universe",
        kind="deb_contents",
        urls=("https://archive.ubuntu.com/ubuntu/dists/noble/Contents-amd64.gz",),
    ),
    Source(
        key="adiscon-v8-stable-focal",
        distro="adiscon-ppa",
        release="focal",
        channel="v8-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/v8-stable/ubuntu/dists/focal/main/binary-amd64/Packages.gz",
        ),
    ),
    Source(
        key="adiscon-v8-stable-jammy",
        distro="adiscon-ppa",
        release="jammy",
        channel="v8-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/v8-stable/ubuntu/dists/jammy/main/binary-amd64/Packages.gz",
        ),
    ),
    Source(
        key="adiscon-v8-stable-noble",
        distro="adiscon-ppa",
        release="noble",
        channel="v8-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/v8-stable/ubuntu/dists/noble/main/binary-amd64/Packages.gz",
        ),
    ),
    Source(
        key="adiscon-daily-stable-focal",
        distro="adiscon-ppa",
        release="focal",
        channel="daily-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/daily-stable/ubuntu/dists/focal/main/binary-amd64/Packages.gz",
        ),
    ),
    Source(
        key="adiscon-daily-stable-jammy",
        distro="adiscon-ppa",
        release="jammy",
        channel="daily-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/daily-stable/ubuntu/dists/jammy/main/binary-amd64/Packages.gz",
        ),
    ),
    Source(
        key="adiscon-daily-stable-noble",
        distro="adiscon-ppa",
        release="noble",
        channel="daily-stable",
        kind="deb_packages",
        urls=(
            "https://ppa.launchpadcontent.net/adiscon/daily-stable/ubuntu/dists/noble/main/binary-amd64/Packages.gz",
        ),
    ),
)

FEDORA_SOURCES = (
    Source(
        key="fedora-rawhide",
        distro="fedora",
        release="rawhide",
        channel="everything",
        kind="fedora_filelists",
        urls=("https://dl.fedoraproject.org/pub/fedora/linux/development/rawhide/Everything/x86_64/os/repodata/repomd.xml",),
    ),
)

ALPINE_SOURCES = (
    Source(
        key="alpine-v3.21",
        distro="alpine",
        release="v3.21",
        channel="main+community",
        kind="alpine_apk",
        urls=(
            "https://dl-cdn.alpinelinux.org/alpine/v3.21/main/x86_64/",
            "https://dl-cdn.alpinelinux.org/alpine/v3.21/community/x86_64/",
        ),
    ),
    Source(
        key="alpine-v3.22",
        distro="alpine",
        release="v3.22",
        channel="main+community",
        kind="alpine_apk",
        urls=(
            "https://dl-cdn.alpinelinux.org/alpine/v3.22/main/x86_64/",
            "https://dl-cdn.alpinelinux.org/alpine/v3.22/community/x86_64/",
        ),
    ),
    Source(
        key="alpine-edge",
        distro="alpine",
        release="edge",
        channel="main+community",
        kind="alpine_apk",
        urls=(
            "https://dl-cdn.alpinelinux.org/alpine/edge/main/x86_64/",
            "https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/",
        ),
    ),
)

SOURCES = DEB_CONTENT_SOURCES + FEDORA_SOURCES + ALPINE_SOURCES


def fetch_url(url: str, cache_dir: Path, errors: list[dict[str, str]]) -> bytes | None:
    parsed_url = urllib.parse.urlparse(url)
    if parsed_url.scheme != "https":
        errors.append({"url": url, "error": "unsupported URL scheme"})
        return None
    cache_key = hashlib.sha256(url.encode("utf-8")).hexdigest()
    cache_path = cache_dir / cache_key
    if cache_path.exists():
        return cache_path.read_bytes()
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT) as response:  # nosec B310 - URL scheme is checked above.
            data = response.read()
    except (urllib.error.URLError, TimeoutError) as exc:
        errors.append({"url": url, "error": str(exc)})
        return None
    cache_path.write_bytes(data)
    return data


def discover_components(repo_root: Path) -> list[Component]:
    components: dict[str, Component] = {}
    for base, kind in (("plugins", "plugin"), ("contrib", "contrib")):
        base_path = repo_root / base
        if not base_path.exists():
            continue
        for child in sorted(base_path.iterdir()):
            if not child.is_dir() or child.name == "external":
                continue
            c_file = child / f"{child.name}.c"
            if not c_file.exists():
                continue
            components[child.name] = Component(
                name=child.name,
                upstream_path=f"{base}/{child.name}/",
                module_file=f"{child.name}.so",
                kind=kind,
            )

    for tool_module in ("omfile", "ompipe", "omprog", "omstdout", "pmrfc3164", "pmrfc5424"):
        if (repo_root / "tools" / f"{tool_module}.c").exists():
            components[tool_module] = Component(
                name=tool_module,
                upstream_path=f"tools/{tool_module}.c",
                module_file=f"{tool_module}.so",
                kind="legacy-built-in",
            )

    return list(sorted(components.values(), key=lambda c: c.name))


def empty_source_result(source: Source) -> dict[str, Any]:
    return {
        "distro": source.distro,
        "release": source.release,
        "channel": source.channel,
        "metadata_kind": source.kind,
        "source_urls": list(source.urls),
        "status": "unknown",
        "error_count": 0,
        "components": {},
    }


def record_match(result: dict[str, Any], component: Component, package: str, path: str, source_url: str) -> None:
    entry = result["components"].setdefault(
        component.name,
        {
            "status": "shipped",
            "packages": [],
        },
    )
    candidate = {
        "package": package,
        "path": path,
        "source_url": source_url,
    }
    if candidate not in entry["packages"]:
        entry["packages"].append(candidate)


def finalize_source_result(result: dict[str, Any], components: list[Component]) -> None:
    for component in components:
        result["components"].setdefault(component.name, {"status": "not_shipped", "packages": []})
    shipped = sum(1 for entry in result["components"].values() if entry["status"] == "shipped")
    if result["status"] == "unknown" and result["error_count"] == len(result["source_urls"]):
        return
    result["status"] = "ok"
    result["component_count"] = len(components)
    result["shipped_component_count"] = shipped


def parse_deb_contents(source: Source, components: list[Component], cache_dir: Path) -> dict[str, Any]:
    result = empty_source_result(source)
    module_files = {component.module_file: component for component in components}
    errors: list[dict[str, str]] = []
    for url in source.urls:
        data = fetch_url(url, cache_dir, errors)
        if data is None:
            continue
        with gzip.GzipFile(fileobj=io.BytesIO(data)) as gz:
            lines = io.TextIOWrapper(gz, encoding="utf-8", errors="replace")
            for line in lines:
                line = line.rstrip("\n")
                if "/rsyslog/" not in line or ".so" not in line:
                    continue
                parts = line.split()
                if len(parts) < 2:
                    continue
                path, package_field = parts[0], parts[-1]
                module = os.path.basename(path)
                component = module_files.get(module)
                if component is None:
                    continue
                packages = [pkg.split("/")[-1] for pkg in package_field.split(",")]
                for package in packages:
                    record_match(result, component, package, path, url)
    result["errors"] = errors
    result["error_count"] = len(errors)
    finalize_source_result(result, components)
    return result


def iter_deb_package_records(data: bytes) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    current: dict[str, str] = {}
    last_key: str | None = None
    with gzip.GzipFile(fileobj=io.BytesIO(data)) as gz:
        lines = io.TextIOWrapper(gz, encoding="utf-8", errors="replace")
        for raw_line in lines:
            raw_line = raw_line.rstrip("\n")
            if not raw_line:
                if current:
                    records.append(current)
                    current = {}
                    last_key = None
                continue
            if raw_line.startswith(" ") and last_key:
                current[last_key] += "\n" + raw_line
                continue
            if ":" not in raw_line:
                continue
            key, value = raw_line.split(":", 1)
            current[key] = value.strip()
            last_key = key
    if current:
        records.append(current)
    return records


def parse_deb_packages(data: bytes) -> list[dict[str, str]]:
    return iter_deb_package_records(data)


def deb_repo_root(packages_url: str) -> str:
    if "/dists/" in packages_url:
        return packages_url.split("/dists/", 1)[0] + "/"
    return packages_url.rsplit("/", 1)[0] + "/"


def ar_members(data: bytes) -> dict[str, bytes]:
    if not data.startswith(b"!<arch>\n"):
        return {}
    members: dict[str, bytes] = {}
    offset = 8
    while offset + 60 <= len(data):
        header = data[offset : offset + 60]
        offset += 60
        name = header[0:16].decode("utf-8", errors="replace").strip().rstrip("/")
        size_raw = header[48:58].decode("ascii", errors="replace").strip()
        try:
            size = int(size_raw)
        except ValueError:
            break
        payload = data[offset : offset + size]
        offset += size + (size % 2)
        members[name] = payload
    return members


def zstd_decompress(data: bytes) -> bytes | None:
    try:
        proc = subprocess.run(
            ["zstd", "-dc"],
            input=data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except FileNotFoundError:
        return None
    if proc.returncode != 0:
        return None
    return proc.stdout


def iter_deb_files(deb_data: bytes) -> list[str]:
    members = ar_members(deb_data)
    data_member_name = next((name for name in members if name.startswith("data.tar.")), None)
    if data_member_name is None:
        return []
    data_member = members[data_member_name]
    if data_member_name.endswith(".zst"):
        data_member = zstd_decompress(data_member) or b""
    files: list[str] = []
    try:
        with tarfile.open(fileobj=io.BytesIO(data_member), mode="r:*") as tf:
            for member in tf.getmembers():
                if member.isfile() or member.issym():
                    files.append("/" + member.name.lstrip("./"))
    except tarfile.TarError:
        return files
    return files


def parse_deb_package_files(source: Source, components: list[Component], cache_dir: Path) -> dict[str, Any]:
    result = empty_source_result(source)
    module_files = {component.module_file: component for component in components}
    errors: list[dict[str, str]] = []
    for packages_url in source.urls:
        data = fetch_url(packages_url, cache_dir, errors)
        if data is None:
            continue
        root_url = deb_repo_root(packages_url)
        for record in parse_deb_packages(data):
            package_name = record.get("Package", "")
            filename = record.get("Filename", "")
            if "rsyslog" not in package_name or not filename:
                continue
            deb_url = urllib.parse.urljoin(root_url, filename)
            deb_data = fetch_url(deb_url, cache_dir, errors)
            if deb_data is None:
                continue
            for path in iter_deb_files(deb_data):
                if "/rsyslog/" not in path or not path.endswith(".so"):
                    continue
                component = module_files.get(os.path.basename(path))
                if component is not None:
                    record_match(result, component, package_name, path, packages_url)
    result["errors"] = errors
    result["error_count"] = len(errors)
    finalize_source_result(result, components)
    return result


def repodata_location(repomd_url: str, repomd_xml: bytes, data_type: str) -> str | None:
    root = ET.fromstring(repomd_xml)
    ns = {"repo": "http://linux.duke.edu/metadata/repo"}
    for data in root.findall("repo:data", ns):
        if data.attrib.get("type") != data_type:
            continue
        location = data.find("repo:location", ns)
        if location is not None:
            href = location.attrib.get("href")
            if href:
                repo_root = repomd_url.split("/repodata/", 1)[0] + "/"
                return urllib.parse.urljoin(repo_root, href)
    return None


def parse_fedora_filelists(source: Source, components: list[Component], cache_dir: Path) -> dict[str, Any]:
    result = empty_source_result(source)
    module_files = {component.module_file: component for component in components}
    errors: list[dict[str, str]] = []
    for repomd_url in source.urls:
        repomd = fetch_url(repomd_url, cache_dir, errors)
        if repomd is None:
            continue
        filelists_url = repodata_location(repomd_url, repomd, "filelists")
        if filelists_url is None:
            errors.append({"url": repomd_url, "error": "filelists metadata not found"})
            continue
        data = fetch_url(filelists_url, cache_dir, errors)
        if data is None:
            continue
        if filelists_url.endswith(".gz"):
            xml_data = gzip.decompress(data)
        elif filelists_url.endswith(".zst"):
            xml_data = zstd_decompress(data)
            if xml_data is None:
                errors.append({"url": filelists_url, "error": "zstd decompression failed"})
                continue
        else:
            xml_data = data
        for event, elem in ET.iterparse(io.BytesIO(xml_data), events=("end",)):
            if not elem.tag.endswith("package"):
                continue
            package_name = elem.attrib.get("name", "")
            if "rsyslog" not in package_name:
                elem.clear()
                continue
            for file_node in elem:
                if not file_node.tag.endswith("file"):
                    continue
                path = file_node.text or ""
                if "/rsyslog/" not in path or not path.endswith(".so"):
                    continue
                component = module_files.get(os.path.basename(path))
                if component is not None:
                    record_match(result, component, package_name, path, filelists_url)
            elem.clear()
    result["errors"] = errors
    result["error_count"] = len(errors)
    finalize_source_result(result, components)
    return result


def parse_apkindex(data: bytes) -> list[dict[str, str]]:
    packages: list[dict[str, str]] = []
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tf:
        apkindex = tf.extractfile("APKINDEX")
        if apkindex is None:
            return packages
        current: dict[str, str] = {}
        lines = io.TextIOWrapper(apkindex, encoding="utf-8", errors="replace")
        for raw_line in lines:
            raw_line = raw_line.rstrip("\r\n")
            if not raw_line:
                if current:
                    packages.append(current)
                    current = {}
                continue
            if ":" not in raw_line:
                continue
            key, value = raw_line.split(":", 1)
            current[key] = value
        if current:
            packages.append(current)
    return packages


def iter_apk_files(apk_data: bytes) -> list[str]:
    files: list[str] = []
    try:
        with tarfile.open(fileobj=io.BytesIO(apk_data), mode="r:*") as tf:
            for member in tf.getmembers():
                if member.isfile() or member.issym():
                    files.append("/" + member.name.lstrip("./"))
    except tarfile.TarError:
        return files
    return files


def parse_alpine_apk(source: Source, components: list[Component], cache_dir: Path) -> dict[str, Any]:
    result = empty_source_result(source)
    module_files = {component.module_file: component for component in components}
    errors: list[dict[str, str]] = []
    for base_url in source.urls:
        index_url = urllib.parse.urljoin(base_url, "APKINDEX.tar.gz")
        index_data = fetch_url(index_url, cache_dir, errors)
        if index_data is None:
            continue
        for pkg in parse_apkindex(index_data):
            package_name = pkg.get("P", "")
            version = pkg.get("V", "")
            arch = pkg.get("A", "x86_64")
            if "rsyslog" not in package_name or not version:
                continue
            apk_url = urllib.parse.urljoin(base_url, f"{package_name}-{version}.apk")
            apk_data = fetch_url(apk_url, cache_dir, errors)
            if apk_data is None:
                continue
            for path in iter_apk_files(apk_data):
                if "/rsyslog/" not in path or not path.endswith(".so"):
                    continue
                component = module_files.get(os.path.basename(path))
                if component is not None:
                    record_match(result, component, package_name, path, index_url)
    result["errors"] = errors
    result["error_count"] = len(errors)
    finalize_source_result(result, components)
    return result


def generate(repo_root: Path, cache_dir: Path) -> dict[str, Any]:
    components = discover_components(repo_root)
    output: dict[str, Any] = {
        "schema_version": 1,
        "description": "Best-effort support data mapping upstream rsyslog modules to public distro packages.",
        "caveats": [
            "Generated from public package metadata and not authoritative for any distribution.",
            "A shipped module file does not mean the module is loaded by default.",
            "A not_shipped result applies only to searched public repositories, not custom builds or third-party packages.",
            "Confirm downstream-specific claims with the relevant distribution security team before publication.",
        ],
        "component_count": len(components),
        "components": {
            component.name: {
                "upstream_path": component.upstream_path,
                "module_file": component.module_file,
                "kind": component.kind,
            }
            for component in components
        },
        "sources": {},
    }
    for source in SOURCES:
        if source.kind == "deb_contents":
            result = parse_deb_contents(source, components, cache_dir)
        elif source.kind == "deb_packages":
            result = parse_deb_package_files(source, components, cache_dir)
        elif source.kind == "fedora_filelists":
            result = parse_fedora_filelists(source, components, cache_dir)
        elif source.kind == "alpine_apk":
            result = parse_alpine_apk(source, components, cache_dir)
        else:
            raise ValueError(f"unknown source kind: {source.kind}")
        output["sources"][source.key] = result
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".", help="rsyslog repository root")
    parser.add_argument("--output", default="doc/security/distro-component-package-map.json")
    parser.add_argument("--cache-dir", default=None)
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    output_path = repo_root / args.output
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if args.cache_dir:
        cache_dir = Path(args.cache_dir)
        cache_dir.mkdir(parents=True, exist_ok=True)
        data = generate(repo_root, cache_dir)
    else:
        with tempfile.TemporaryDirectory() as tmp:
            data = generate(repo_root, Path(tmp))

    output_path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
