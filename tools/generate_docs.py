#!/usr/bin/env python3
"""Generate Hugo Symbols vault pages from public single-header declarations."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys

from site_paths import SITE

ROOT = Path(__file__).resolve().parents[1]
# Rend2 belongs to the separate godstack-pro repository.
LIBRARIES = ("Cast", "Cool", "Fuse", "Grit", "Peak", "Poof", "Rend", "Type", "Wire")
BEGIN = "<!-- BEGIN GENERATED API -->"
END = "<!-- END GENERATED API -->"
# Strings and comments must be consumed before punctuation or directives.
TOKEN = re.compile(
    r'(?P<comment>/\*.*?\*/|//[^\n]*)'
    r'|(?P<string>"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')'
    r'|(?P<directive>^[ \t]*\#(?:[^\n\\]|\\[^\n]|\\\n)*)'
    r'|(?P<word>[A-Za-z_]\w*)|(?P<space>[^\S\n]+|\n)|(?P<punct>.)',
    re.DOTALL | re.MULTILINE,
)


@dataclass
class Entry:
    name: str
    kind: str
    declaration: str
    description: str
    line: int


def public_source(source, library):
    """Exclude unboxing examples and implementation, without preprocessing."""
    if library == "Poof":
        end = source.index("/* Implementation */")
        return source[:end], 0
    guard = re.search(r"^#ifndef _?" + library.upper() + r"_H_?\s*$", source, re.MULTILINE)
    if not guard:
        raise ValueError(f"{library}: public header guard not found")
    implementation = re.search(
        r"^#if(?:def)?\b[^\n]*\b" + library.upper() + r"_IMPLEMENTATION\b",
        source[guard.end():], re.MULTILINE,
    )
    if not implementation:
        raise ValueError(f"{library}: implementation boundary not found")
    start = guard.end()
    return source[start:start + implementation.start()], source[:start].count("\n")


def clean_comment(text):
    if text.startswith("/*"):
        text = re.sub(r"^/\*[*!]?<?|\*/$", "", text)
        text = re.sub(r"^[ \t]*\* ?", "", text, flags=re.MULTILINE)
    else:
        text = re.sub(r"^//[/!]?<? ?", "", text)
    text = text.strip()
    # A small, readable Doxygen subset; other commands remain verbatim.
    text = re.sub(r"(?:@|\\)brief\s*", "", text)
    text = re.sub(r"(?:@|\\)param(?:\[([^]]+)\])?\s+(\w+)\s*",
                  lambda m: "\n- **`" + m[2] + "`" + (" (" + m[1] + ")" if m[1] else "") + ":** ", text)
    for tag, label in (("returns?", "Returns"), ("note", "Note"), ("warning", "Warning"), ("see", "See also")):
        text = re.sub(r"(?:@|\\)" + tag + r"\b\s*", "\n\n**" + label + ":** ", text)
    text = re.sub(r"(?:@|\\)code\b[^\n]*", "\n```c", text)
    text = re.sub(r"(?:@|\\)endcode\b", "```\n", text)
    return text.strip()


def extract(source, library):
    public, base_line = public_source(source, library)
    entries = []
    prefix = "rend" if library == "Rend2" else library.lower()
    parts, comments = [], []
    depth = 0
    line = base_line + 1
    start_line = line
    last_end_line = -1
    for token in TOKEN.finditer(public):
        text, kind = token[0], token.lastgroup
        current_line = line
        line += text.count("\n")
        if kind == "comment":
            if not parts:
                if entries and current_line == last_end_line:
                    extra = clean_comment(text)
                    entries[-1].description = "\n".join(filter(None, (entries[-1].description, extra)))
                else:
                    comments.append(clean_comment(text))
            # Keep member comments in aggregate declarations.
            elif depth:
                parts.append(text)
            continue
        if kind == "directive":
            macro = re.match(r"\s*#\s*define\s+(\w+)\(", text)
            if macro and macro[1].lower().startswith(prefix + "_"):
                entries.append(Entry(macro[1], "Macros", text.strip(), "\n".join(comments), current_line))
            comments = []
            continue
        if kind == "space" and not parts:
            continue
        if not parts:
            start_line = current_line
        parts.append(text)
        if kind == "punct" and text in "{([":
            depth += 1
        elif kind == "punct" and text in "})]":
            depth -= 1
        if kind != "punct" or text != ";" or depth:
            continue
        declaration = "".join(parts).strip()
        name, category = "", ""
        if declaration.startswith("typedef"):
            callback = re.search(r"\(\s*\*\s*(\w+)\s*\)", declaration)
            tail = re.search(r"(\w+)\s*(?:\[[^]]*\])?\s*;$", declaration)
            name = callback[1] if callback else tail[1] if tail else ""
            category = "Types"
        elif re.match(r"(?:enum|struct|union)\s+\w+\s*\{", declaration):
            name = re.match(r"\w+\s+(\w+)", declaration)[1]
            category = "Types"
        else:
            function = re.search(r"\b(" + prefix + r"_\w+)\s*\(", declaration)
            if function:
                name, category = function[1], "Functions"
        if name:
            entries.append(Entry(name, category, declaration, "\n".join(comments).strip(), start_line))
        parts, comments = [], []
        last_end_line = current_line
    return entries


def generated_section(library, entries):
    if not entries:
        raise ValueError(f"{library}: no public declarations extracted")
    lines = [BEGIN, "", "## API reference", "",
             f"Generated from `{library}.h` by `tools/generate_docs.py`. Edit the header comments, not this section.", "",
             "Declarations from all platform branches are listed; this is not a preprocessed API.", ""]
    anchors = {}
    for category in ("Types", "Macros", "Functions"):
        selected = [e for e in entries if e.kind == category]
        if not selected:
            continue
        lines += [f"### {category}", ""]
        for entry in selected:
            fence = "`" * max(3, max((len(m[0]) + 1 for m in re.finditer(r"`+", entry.declaration)), default=3))
            base = entry.name.lower()
            occurrence = anchors.get(base, 0)
            anchors[base] = occurrence + 1
            anchor = base + (f"-{occurrence}" if occurrence else "")
            lines += [f"#### {entry.name} {{#{anchor}}}", "", f"Source: `{library}.h:{entry.line}`", "",
                      fence + "c", entry.declaration, fence, ""]
            if entry.description:
                lines += [entry.description, ""]
    return "\n".join(lines + [END])


def symbol_records(library, entries):
    """Names and explicit anchors shared by the vault and its search records."""
    records, counts = [], {}
    for category in ("Types", "Macros", "Functions"):
        for entry in (e for e in entries if e.kind == category):
            base = entry.name.lower()
            occurrence = counts.get(base, 0)
            counts[base] = occurrence + 1
            anchor = base + (f"-{occurrence}" if occurrence else "")
            records.append({"id": f"symbol:{library.lower()}:{anchor}", "name": entry.name,
                            "library": library, "kind": category, "anchor": anchor,
                            "description": entry.description or f"{category.rstrip('s')} declared in {library}.h.",
                            "declaration": entry.declaration, "line": entry.line})
    return records


def update_page(existing, library, section):
    if BEGIN in existing or END in existing:
        if existing.count(BEGIN) != 1 or existing.count(END) != 1 or existing.index(BEGIN) > existing.index(END):
            raise ValueError(f"{library}: malformed generated section markers")
        start, end = existing.index(BEGIN), existing.index(END) + len(END)
        return existing[:start] + section + existing[end:]
    if not existing:
        existing = (f"+++\ntitle = '{library} symbols'\n"
                    f"description = 'Types, macros, and functions declared by {library}.h.'\nweight = 70\n"
                    f"[params]\n  id = 'symbols/{library.lower()}'\n  product = '{library}'\n"
                    "  category = 'Symbols'\n  tags = ['API', 'symbols', 'C']\n"
                    "  access = 'free'\n  symbols = true\n  audience = 'Core'\n+++\n\n"
                    + f"# {library} symbols\n\n"
                    + f"For setup and API workflows, read the [{library} guide](/docs/{library.lower()}/). "
                    + "This vault lists individual declarations; it is not a tutorial.\n")
    return existing.rstrip() + "\n\n" + section + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report stale pages without writing")
    parser.add_argument("--site", type=Path, default=SITE, help="website checkout (or GODSTACK_SITE)")
    args = parser.parse_args()
    site = args.site.expanduser().resolve()
    if not site.is_dir():
        print(f"generate_docs: website checkout not found: {site}; set GODSTACK_SITE or --site", file=sys.stderr)
        return 1
    stale = []
    try:
        # Prepare everything before writing so malformed pages do not cause partial updates.
        updates = []
        symbols = []
        for library in LIBRARIES:
            header = ROOT / f"{library}.h"
            page = site / "articles/symbols" / f"{library.lower()}.md"
            existing = page.read_text(encoding="utf-8") if page.exists() else ""
            entries = extract(header.read_text(encoding="utf-8"), library)
            section = generated_section(library, entries)
            symbols.extend(symbol_records(library, entries))
            updated = update_page(existing, library, section)
            if updated != existing:
                stale.append(str(page.relative_to(site)))
                updates.append((page, updated))
        index = site / "data/symbols.json"
        updated_index = json.dumps(symbols, ensure_ascii=False, indent=2) + "\n"
        existing_index = index.read_text(encoding="utf-8") if index.exists() else ""
        if updated_index != existing_index:
            stale.append(str(index.relative_to(site)))
            updates.append((index, updated_index))
        if not args.check:
            for page, updated in updates:
                page.parent.mkdir(parents=True, exist_ok=True)
                page.write_text(updated, encoding="utf-8")
    except (OSError, ValueError) as error:
        print(f"generate_docs: {error}", file=sys.stderr)
        return 1
    for path in stale:
        print(("stale: " if args.check else "updated: ") + path)
    return int(args.check and bool(stale))


if __name__ == "__main__":
    sys.exit(main())
