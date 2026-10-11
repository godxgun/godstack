#!/usr/bin/env python3
"""Validate the built Hugo site against the public documentation contract.

Usage: python3 tools/test_site.py PATH-TO-HUGO-OUTPUT [--base-path /prefix/]
The deployment prefix is inferred from the home page's canonical URL when omitted.
Only the Python standard library is required.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from html.parser import HTMLParser
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

LIBRARIES = ("cast", "cool", "fuse", "grit", "peak", "poof", "rend", "type", "wire")
WEB_DEMOS = ("grit", "snake")
DEMO_PAGES = WEB_DEMOS
ROOT_PAGES = {
    "ai": ("/ai/", "/ai.md"),
    "license": ("/license/", "/license.md"),
    "documentation": ("/docs/", "/docs/index.md"),
    "guides": ("/guides/", "/guides/index.md"),
    "symbols": ("/symbols/", "/symbols/index.md"),
    "demos": ("/demos/", "/demos/index.md"),
}
REQUIRED_PAGE_RECORDS = {name: (f"/docs/{name}/", f"/docs/{name}.md") for name in LIBRARIES}
REQUIRED_PAGE_RECORDS.update({"getting-started": ("/guides/getting-started/", "/guides/getting-started.md")})
REQUIRED_PAGE_RECORDS.update(ROOT_PAGES)
REQUIRED_PAGE_RECORDS.update({f"symbols/{name}": (f"/symbols/{name}/", f"/symbols/{name}.md") for name in LIBRARIES})
REQUIRED_PAGE_RECORDS.update({f"demos/{name}": (f"/demos/{name}/", f"/demos/{name}.md") for name in DEMO_PAGES})
REQUIRED_HTML = (
    "index.html",
    "guides/getting-started/index.html",
    *(f"docs/{name}/index.html" for name in LIBRARIES),
    *(f"{route.strip('/')}/index.html" for route, _ in ROOT_PAGES.values()),
    *(f"symbols/{name}/index.html" for name in LIBRARIES),
    *(f"demos/{name}/index.html" for name in DEMO_PAGES),
)
REQUIRED_MARKDOWN = tuple(markdown.lstrip("/") for _, markdown in REQUIRED_PAGE_RECORDS.values())
REQUIRED_DEMO_RUNS = tuple(f"demos/{name}/run.html" for name in WEB_DEMOS)
REQUIRED_DEMO_ARTIFACTS = tuple(f"demos/{name}/{artifact}" for name in WEB_DEMOS for artifact in ("demo.js", "manifest.json"))
REQUIRED_DEMO_THUMBNAILS = tuple(f"images/{name}-demo.png" for name in WEB_DEMOS)
REQUIRED_RESOURCES = ("llms.txt", "llms-small.txt", "llms-full.txt", "catalog.json", "search.json", "agent/SKILL.md", "agent/AGENTS.md")
CATALOG_FIELDS = {"id", "title", "description", "categories", "tags", "url", "markdown_url", "access"}
SEARCH_FIELDS = {"id", "title", "description", "category", "product", "tags", "url", "content"}


class Links(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.refs: list[tuple[str, str]] = []
        self.anchors: set[str] = set()
        self.text: list[str] = []
        self.canonical: str | None = None
        self.images: list[str] = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "link" and "canonical" in (attrs.get("rel", "").lower().split()):
            self.canonical = attrs.get("href")
        for key in ("href", "src", "poster", "action"):
            if attrs.get(key):
                self.refs.append((key, attrs[key]))
        if tag == "img" and attrs.get("src"):
            self.images.append(attrs["src"])
        for key in ("id", "name"):
            if attrs.get(key):
                self.anchors.add(attrs[key])

    def handle_data(self, data):
        self.text.append(data)


def fail(errors: list[str], message: str):
    errors.append(message)


def resolve_file(root: Path, relative: str) -> Path | None:
    """Map a URL path to a public file, allowing Hugo's directory index URLs."""
    path = unquote(relative).lstrip("/")
    if not path:
        path = "index.html"
    candidate = root.joinpath(*PurePosixPath(path).parts)
    options = [candidate]
    if path.endswith("/") or not Path(path).suffix:
        options.append(candidate / "index.html")
    return next((p for p in options if p.is_file()), None)


def normalize_base_path(value: str) -> str:
    parsed = urlsplit(value)
    if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment:
        raise ValueError("base path must be a URL path without origin, query, or fragment")
    path = parsed.path or "/"
    return "/" + path.strip("/") + "/" if path.strip("/") else "/"


def strip_base_path(path: str, base_path: str) -> str | None:
    """Convert a deployment URL path to a path relative to Hugo's output root."""
    path = unquote(path)
    if base_path == "/":
        return path or "/"
    prefix = base_path.rstrip("/")
    if path == prefix:
        return "/"
    if path.startswith(base_path):
        return "/" + path[len(base_path):]
    return None


def site_path(path: str, base_path: str) -> str:
    if base_path == "/":
        return path
    return base_path.rstrip("/") + path


def resolve_site_file(root: Path, path: str, base_path: str) -> Path | None:
    relative = strip_base_path(path, base_path)
    return resolve_file(root, relative) if relative is not None else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Hugo public/output directory")
    parser.add_argument("--base-path", help="deployment URL prefix (inferred from the canonical home URL when omitted)")
    args = parser.parse_args()
    root = args.output.resolve()
    errors: list[str] = []
    if not root.is_dir():
        print(f"FAIL: output directory does not exist: {root}", file=sys.stderr)
        return 2

    if args.base_path is not None:
        try:
            base_path = normalize_base_path(args.base_path)
        except ValueError as exc:
            parser.error(str(exc))
    else:
        base_path = "/"
        home_path = root / "index.html"
        if home_path.is_file():
            home_probe = Links()
            try:
                home_probe.feed(home_path.read_text(encoding="utf-8"))
                if home_probe.canonical:
                    base_path = normalize_base_path(urlsplit(home_probe.canonical).path)
            except (OSError, UnicodeError, ValueError):
                pass

    for relative in REQUIRED_HTML + REQUIRED_MARKDOWN + REQUIRED_DEMO_RUNS + REQUIRED_DEMO_ARTIFACTS + REQUIRED_DEMO_THUMBNAILS + REQUIRED_RESOURCES:
        if not (root / relative).is_file():
            fail(errors, f"missing required output: /{relative}")
    for relative in REQUIRED_DEMO_THUMBNAILS:
        image = root / relative
        if not image.is_file():
            continue
        try:
            data = image.read_bytes()
            if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
                fail(errors, f"/{relative} is not a valid PNG image")
            else:
                width, height = struct.unpack(">II", data[16:24])
                if width < 1 or height < 1 or b"IEND" not in data[-16:]:
                    fail(errors, f"/{relative} has invalid PNG dimensions or is incomplete")
        except OSError as exc:
            fail(errors, f"cannot read /{relative}: {exc}")

    # Verify WebAssembly artifact manifests against every emitted file. Runtime
    # browser checks execute the demos; these hashes also catch stale/missing bundles.
    for demo in WEB_DEMOS:
        manifest_path = root / "demos" / demo / "manifest.json"
        if not manifest_path.is_file():
            continue
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (UnicodeError, json.JSONDecodeError) as exc:
            fail(errors, f"/demos/{demo}/manifest.json is not valid UTF-8 JSON: {exc}")
            continue
        if not isinstance(manifest, dict):
            fail(errors, f"/demos/{demo}/manifest.json must contain an object")
            continue
        if manifest.get("id") != demo or manifest.get("source") != f"demos/{demo}/{'snake' if demo == 'snake' else 'demo'}.c":
            fail(errors, f"/demos/{demo}/manifest.json has incorrect demo identity/source metadata")
        files = manifest.get("files")
        if not isinstance(files, dict):
            fail(errors, f"/demos/{demo}/manifest.json.files must be an object")
            continue
        for required in ("demo.js", "run.html"):
            if required not in files:
                fail(errors, f"/demos/{demo}/manifest.json does not hash required artifact {required}")
        if "-sSINGLE_FILE=1" not in manifest.get("flags", []):
            fail(errors, f"/demos/{demo}/manifest.json does not declare the single-file Wasm build flag")
        for filename, digest in files.items():
            relative = PurePosixPath(filename)
            artifact = root / "demos" / demo / Path(*relative.parts)
            if relative.is_absolute() or ".." in relative.parts:
                fail(errors, f"/demos/{demo}/manifest.json contains unsafe artifact path {filename!r}")
            elif not artifact.is_file():
                fail(errors, f"/demos/{demo}/{filename} is listed in the manifest but missing")
            elif not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
                fail(errors, f"/demos/{demo}/manifest.json has invalid SHA-256 for {filename}")
            elif hashlib.sha256(artifact.read_bytes()).hexdigest() != digest:
                fail(errors, f"/demos/{demo}/{filename} does not match its manifest SHA-256")

    catalog = []
    search = []
    for filename, fields, target in (("catalog.json", CATALOG_FIELDS, catalog), ("search.json", SEARCH_FIELDS, search)):
        path = root / filename
        if not path.is_file():
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (UnicodeError, json.JSONDecodeError) as exc:
            fail(errors, f"/{filename} is not valid UTF-8 JSON: {exc}")
            continue
        if not isinstance(data, list):
            fail(errors, f"/{filename} must contain a JSON array")
            continue
        target.extend(data)
        for i, item in enumerate(data):
            label = f"/{filename}[{i}]"
            if not isinstance(item, dict):
                fail(errors, f"{label} must be an object")
                continue
            missing = fields - item.keys()
            if missing:
                fail(errors, f"{label} missing fields: {', '.join(sorted(missing))}")
            for key in (("categories", "tags") if filename == "catalog.json" else ("tags",)):
                if key in item and not isinstance(item[key], list):
                    fail(errors, f"{label}.{key} must be an array")
            for key in ("id", "title", "description", "url"):
                if key in item and (not isinstance(item[key], str) or not item[key].strip()):
                    fail(errors, f"{label}.{key} must be a non-empty string")
            if filename == "search.json":
                for key in ("category", "product", "content"):
                    if key in item and not isinstance(item[key], str):
                        fail(errors, f"{label}.{key} must be a string")
            if filename == "catalog.json" and item.get("access") != "free":
                fail(errors, f"{label}.access must be 'free'")
            for key in ("markdown_url",) if filename == "catalog.json" else ():
                if key in item and (not isinstance(item[key], str) or not item[key].strip()):
                    fail(errors, f"{label}.{key} must be a non-empty string")

    for filename, entries in (("catalog.json", catalog), ("search.json", search)):
        ids = [item.get("id") for item in entries if isinstance(item, dict) and isinstance(item.get("id"), str)]
        if len(ids) != len(set(ids)):
            fail(errors, f"/{filename} contains duplicate ids")

    catalog_ids = {item.get("id") for item in catalog if isinstance(item, dict) and isinstance(item.get("id"), str)}
    search_ids = {item.get("id") for item in search if isinstance(item, dict) and isinstance(item.get("id"), str)}
    if catalog_ids != search_ids:
        fail(errors, f"catalog/search ID sets differ (catalog-only: {', '.join(sorted(catalog_ids - search_ids)) or 'none'}; search-only: {', '.join(sorted(search_ids - catalog_ids)) or 'none'})")
    missing_ids = set(REQUIRED_PAGE_RECORDS) - catalog_ids
    if missing_ids:
        fail(errors, f"/catalog.json is missing required public page IDs: {', '.join(sorted(missing_ids))}")

    catalog_by_id = {item.get("id"): item for item in catalog if isinstance(item, dict)}
    for item_id, (route, markdown_route) in REQUIRED_PAGE_RECORDS.items():
        item = catalog_by_id.get(item_id)
        if not item:
            continue
        expected_url = site_path(route, base_path)
        expected_markdown_url = site_path(markdown_route, base_path)
        if item.get("url") != expected_url:
            fail(errors, f"/catalog.json entry {item_id} has non-canonical url (expected {expected_url})")
        if item.get("markdown_url") != expected_markdown_url:
            fail(errors, f"/catalog.json entry {item_id} has non-canonical markdown_url (expected {expected_markdown_url})")

    for i, item in enumerate(catalog):
        if not isinstance(item, dict):
            continue
        for key in ("url", "markdown_url"):
            url = item.get(key)
            if not isinstance(url, str):
                continue
            parts = urlsplit(url)
            if parts.scheme or parts.netloc:
                fail(errors, f"/catalog.json[{i}].{key} must be a local canonical path, got {url}")
                continue
            if resolve_site_file(root, parts.path, base_path) is None:
                fail(errors, f"/catalog.json[{i}].{key} points to missing or out-of-prefix output: {url}")
    for i, item in enumerate(search):
        if not isinstance(item, dict) or not isinstance(item.get("url"), str):
            continue
        parts = urlsplit(item["url"])
        if parts.scheme or parts.netloc or resolve_site_file(root, parts.path, base_path) is None:
            fail(errors, f"/search.json[{i}].url points to missing, non-local, or out-of-prefix output: {item['url']}")
        item_id = item.get("id")
        catalog_item = catalog_by_id.get(item_id)
        if catalog_item and item["url"] != catalog_item.get("url"):
            fail(errors, f"/search.json[{i}].url does not match catalog canonical URL for {item_id}")

    expected_markdown_urls = [markdown for _, markdown in REQUIRED_PAGE_RECORDS.values()]
    for expected in expected_markdown_urls:
        published_url = site_path(expected, base_path)
        relative = strip_base_path(published_url, base_path)
        if relative is None or not (root / relative.lstrip("/")).is_file():
            continue
        if not any(isinstance(item, dict) and item.get("markdown_url") == published_url for item in catalog):
            fail(errors, f"catalog.json has no entry with markdown_url {published_url}")

    # Preserve canonical local paths where represented in JSON. Walk every generated
    # HTML page and check file paths, plus fragment identifiers on local HTML pages.
    pages = list(root.rglob("*.html"))
    parsed: dict[Path, Links] = {}
    for html in pages:
        try:
            doc = Links()
            doc.feed(html.read_text(encoding="utf-8"))
            parsed[html] = doc
        except (OSError, UnicodeError) as exc:
            fail(errors, f"cannot read {html.relative_to(root)}: {exc}")

    gallery = parsed.get(root / "demos" / "index.html")
    if gallery:
        for name in DEMO_PAGES:
            expected = site_path(f"/images/{name}-demo.png", base_path)
            if expected not in gallery.images:
                fail(errors, f"/demos/index.html does not use the required {name} PNG thumbnail {expected}")

    # Every rendered public section page must have a matching catalog record and
    # local Markdown representation, including additional future pages in these sections.
    public_sections = {"ai", "docs", "demos", "guides", "license", "symbols"}
    catalog_pages = [item for item in catalog if isinstance(item, dict) and isinstance(item.get("url"), str) and not str(item.get("id", "")).startswith("symbol:")]
    for html, doc in parsed.items():
        relative = html.relative_to(root).as_posix()
        if relative.endswith("/index.html"):
            section = relative.split("/", 1)[0]
            if section in public_sections:
                route = "/" + relative.removesuffix("index.html")
                matching = next((item for item in catalog_pages if item.get("url") == site_path(route, base_path)), None)
                if not matching:
                    fail(errors, f"public HTML page {relative} has no catalog page record for {route}")
                else:
                    markdown_url = matching.get("markdown_url")
                    if not isinstance(markdown_url, str) or resolve_site_file(root, urlsplit(markdown_url).path, base_path) is None:
                        fail(errors, f"public HTML page {relative} has no valid local Markdown export in catalog")
        for attr, ref in doc.refs:
            u = urlsplit(ref)
            if u.scheme or u.netloc or ref.startswith(("data:", "mailto:", "tel:", "javascript:")):
                continue
            if not u.path and not u.fragment:
                continue
            raw_path = u.path
            if raw_path.startswith("/"):
                site_relative = strip_base_path(raw_path, base_path)
                if site_relative is None:
                    fail(errors, f"local {attr} in {html.relative_to(root)} is outside deployment base path {base_path}: {ref}")
                    continue
                rel = site_relative.lstrip("/")
            else:
                rel = str(PurePosixPath(html.relative_to(root).as_posix()).parent / raw_path)
                rel = str(PurePosixPath(rel))
            target = resolve_file(root, rel)
            if target is None:
                fail(errors, f"broken local {attr} in {html.relative_to(root)}: {ref}")
                continue
            if u.fragment and target.suffix.lower() == ".html":
                target_doc = parsed.get(target)
                if target_doc is None:
                    target_doc = Links()
                    try:
                        target_doc.feed(target.read_text(encoding="utf-8"))
                    except (OSError, UnicodeError):
                        continue
                    parsed[target] = target_doc
                if unquote(u.fragment) not in target_doc.anchors:
                    fail(errors, f"missing anchor #{u.fragment} in {target.relative_to(root)} (linked from {html.relative_to(root)})")

    for i, item in enumerate(catalog):
        if not isinstance(item, dict) or not isinstance(item.get("url"), str):
            continue
        parts = urlsplit(item["url"])
        if not parts.fragment:
            continue
        target = resolve_site_file(root, parts.path, base_path)
        if target is None or target.suffix.lower() != ".html":
            fail(errors, f"/catalog.json[{i}].url fragment does not target a local HTML page: {item['url']}")
            continue
        target_doc = parsed.get(target)
        if target_doc is None:
            target_doc = Links()
            try:
                target_doc.feed(target.read_text(encoding="utf-8"))
                parsed[target] = target_doc
            except (OSError, UnicodeError):
                continue
        if unquote(parts.fragment) not in target_doc.anchors:
            fail(errors, f"/catalog.json[{i}].url references missing anchor #{parts.fragment} in {target.relative_to(root)}")

    for name in LIBRARIES:
        page = parsed.get(root / "docs" / name / "index.html")
        target = site_path(f"/symbols/{name}/", base_path)
        if page and not any(key == "href" and urlsplit(ref).path == target for key, ref in page.refs):
            fail(errors, f"/docs/{name}/index.html has no link to symbol reference {target}")

    home = parsed.get(root / "index.html")
    if home:
        format_paths = {unquote(urlsplit(ref).path).rstrip("/").rsplit("/", 1)[-1] for _, ref in home.refs}
        format_routes = {"Search": "search.json", "Catalog": "catalog.json", "LLMs": "llms.txt", "LLMsSmall": "llms-small.txt", "LLMsFull": "llms-full.txt"}
        for name, route in format_routes.items():
            if route not in format_paths:
                fail(errors, f"home page does not expose the {name} custom format via /{route}")

    # Public generated API blocks and their header line references are expected in
    # llms-full.txt. Reject actual workstation/credential/private-content leaks only.
    full_path = root / "llms-full.txt"
    if full_path.is_file():
        text = full_path.read_text(encoding="utf-8", errors="replace")
        starts = re.findall(r"^<!-- BEGIN PAGE: (.+?) -->$", text, re.MULTILINE)
        ends = re.findall(r"^<!-- END PAGE: (.+?) -->$", text, re.MULTILINE)
        expected_routes = {item.get("url") for item in catalog if isinstance(item, dict) and isinstance(item.get("url"), str) and not str(item.get("id", "")).startswith("symbol:")}
        if set(starts) != expected_routes or len(starts) != len(expected_routes):
            fail(errors, "/llms-full.txt page boundaries do not match catalog public page URLs")
        if starts != ends:
            fail(errors, "/llms-full.txt BEGIN/END page boundaries are unbalanced or out of order")

    leak_patterns = {
        "workstation path": r"/home/[^\s)]+",
        "AWS access key": r"\bAKIA[0-9A-Z]{16}\b",
        "GitHub token": r"\b(?:gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b",
        "private key": r"-----BEGIN (?:RSA |OPENSSH |EC |DSA )?PRIVATE KEY-----",
        "private fixture sentinel": r"PRIVATE_CONTENT_SENTINEL",
    }
    for name in ("llms.txt", "llms-small.txt", "llms-full.txt", "agent/SKILL.md", "agent/AGENTS.md"):
        path = root / name
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for description, pattern in leak_patterns.items():
            if re.search(pattern, text, re.IGNORECASE | re.MULTILINE):
                fail(errors, f"/{name} contains a possible {description}")

    if errors:
        print(f"FAIL: {len(errors)} site contract error(s) in {root}:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"PASS: validated {root} (base path {base_path}): {len(REQUIRED_HTML)} required HTML routes, {len(REQUIRED_MARKDOWN)} Markdown outputs, {len(REQUIRED_RESOURCES)} resources, {len(pages)} HTML pages and all local links/anchors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
