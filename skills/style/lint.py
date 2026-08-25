#!/usr/bin/env python3
"""C style linter using tree-sitter.

Usage:
  python3 lint.py PATH [PATH ...]

Uses `uv run` for tree-sitter (see pyproject.toml).
Prints compact diagnostics. Exit 1 if issues remain.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

SKIP_DIRS = {".git", ".venv", "unused code"}
C_EXTS = {".c", ".h"}


def ensure_parser():
    if os.environ.get("_STYLE_LINT_UV") == "1":
        return
    try:
        import tree_sitter  # noqa: F401
        import tree_sitter_c  # noqa: F401
        return
    except ImportError:
        pass
    os.environ["_STYLE_LINT_UV"] = "1"
    os.execvp("uv", ["uv", "run", "--project", str(HERE), "python", __file__, *sys.argv[1:]])


ensure_parser()

from tree_sitter import Language, Parser  # noqa: E402
import tree_sitter_c  # noqa: E402

PARSER = Parser(Language(tree_sitter_c.language()))


class Issue:
    __slots__ = ("path", "line", "col", "rule", "msg")

    def __init__(self, path, node, rule, msg):
        self.path = path
        self.line = node.start_point.row + 1
        self.col = node.start_point.column + 1
        self.rule = rule
        self.msg = msg

    def __str__(self):
        return "%s:%d:%d: %s: %s" % (self.path, self.line, self.col, self.rule, self.msg)


def collect_paths(args):
    out = []
    for p in args:
        path = Path(p)
        if path.is_dir():
            for fp in path.rglob("*"):
                if fp.is_file() and fp.suffix.lower() in C_EXTS:
                    if not any(part in SKIP_DIRS for part in fp.parts):
                        out.append(fp)
        elif path.suffix.lower() in C_EXTS:
            out.append(path)
    return out


def src_slice(src, node):
    return src[node.start_byte:node.end_byte]


def walk(node):
    stack = [node]
    while stack:
        n = stack.pop()
        yield n
        kids = n.children
        for i in range(len(kids) - 1, -1, -1):
            stack.append(kids[i])


def field(node, name):
    return node.child_by_field_name(name)


def fn_name_node(decl):
    n = decl
    while n is not None:
        if n.type == "identifier":
            return n
        n = field(n, "declarator")
    return None


def check_ctrl_spacing(src, node, issues, path, kw):
    kids = node.children
    if not kids:
        return
    key = kids[0]
    paren = None
    for c in kids[1:]:
        if c.type in ("(", "parenthesized_expression") or src_slice(src, c).startswith(b"("):
            paren = c
            break
    if paren is None:
        return
    if src[key.end_byte:paren.start_byte] != b" ":
        issues.append(Issue(path, key, "keyword", "want one space after %s" % kw))
    body = src[paren.start_byte:paren.end_byte]
    if body.startswith(b"( "):
        issues.append(Issue(path, paren, "keyword", "no space after ("))
    if body.endswith(b" )"):
        issues.append(Issue(path, paren, "keyword", "no space before )"))


def lint_includes(path, src, root, issues):
    sys_inc = []
    seen_non_inc = False
    saw_loc = False
    for c in root.children:
        if c.type == "comment":
            continue
        if c.type == "preproc_include":
            if seen_non_inc:
                continue
            t = src_slice(src, c).decode("utf-8", "replace")
            m = re.search(r'[<"]([^>"]+)[>"]', t)
            if not m:
                continue
            if "<" in t:
                sys_inc.append(m.group(1))
                if saw_loc:
                    issues.append(Issue(path, c, "include", "system includes must come before local includes"))
            else:
                saw_loc = True
            continue
        if c.type.startswith("preproc_"):
            continue
        seen_non_inc = True
    if sys_inc and sys_inc != sorted(sys_inc, key=str.lower):
        issues.append(Issue(path, root, "include", "system includes not alphabetical: %s" % ", ".join(sys_inc)))


def lint_if(src, node, lines, issues, path):
    check_ctrl_spacing(src, node, issues, path, "if")
    cons = field(node, "consequence")
    if cons is not None and cons.type == "compound_statement" and cons.children:
        brace = cons.children[0]
        if brace.start_point.row != node.start_point.row:
            issues.append(Issue(path, brace, "brace", "{ of if must be on the same line"))


def lint_switch(src, node, lines, issues, path):
    check_ctrl_spacing(src, node, issues, path, "switch")
    body = field(node, "body")
    if body is None:
        return
    sw_col = node.start_point.column
    for ch in body.children:
        if ch.type == "case_statement" and ch.start_point.column > sw_col:
            issues.append(Issue(path, ch, "switch", "do not indent cases another level"))


def lint_function(src, node, lines, issues, path):
    decl = field(node, "declarator")
    typ = field(node, "type")
    name = fn_name_node(decl) if decl else None
    if typ is not None and name is not None and typ.start_point.row == name.start_point.row:
        issues.append(Issue(path, name, "func", "return type must be on its own line"))
    body = field(node, "body")
    if body is None or body.type != "compound_statement" or not body.children:
        return
    brace = body.children[0]
    if brace.start_point.row == node.start_point.row or (
        name is not None and brace.start_point.row == name.start_point.row
    ):
        issues.append(Issue(path, brace, "func", "function { must be on its own line"))
    if lines[brace.start_point.row].strip() != "{":
        issues.append(Issue(path, brace, "func", "function { must be alone on its line"))


def lint_params(src, node, lines, issues, path):
    if not any(c.type == "parameter_declaration" for c in node.children):
        issues.append(Issue(path, node, "params", "use (void) for empty parameter lists"))


def lint_pointer(src, node, lines, issues, path):
    star = next((c for c in node.children if src_slice(src, c) == b"*"), None)
    if star is None:
        return
    rest = field(node, "declarator")
    if rest is not None and src[star.end_byte:rest.start_byte] != b"":
        issues.append(Issue(path, star, "pointer", "* must be adjacent to the variable name"))
    if star.start_byte > 0 and src[star.start_byte - 1:star.start_byte] not in b" \t\n(&":
        issues.append(Issue(path, star, "pointer", "* must be adjacent to the variable name, not the type"))


def lint_block(src, node, lines, issues, path):
    seen_stmt = False
    for ch in node.children:
        if ch.type in ("{", "}", "comment") or ch.type.startswith("preproc_"):
            continue
        if ch.type in ("declaration", "type_definition"):
            if seen_stmt:
                issues.append(Issue(path, ch, "decl-order", "declarations must be at the top of the block"))
        else:
            seen_stmt = True


CHECKS = {
    "for_statement": lambda src, n, lines, issues, path: check_ctrl_spacing(src, n, issues, path, "for"),
    "while_statement": lambda src, n, lines, issues, path: check_ctrl_spacing(src, n, issues, path, "while"),
    "if_statement": lint_if,
    "switch_statement": lint_switch,
    "function_definition": lint_function,
    "parameter_list": lint_params,
    "pointer_declarator": lint_pointer,
    "compound_statement": lint_block,
}


def lint_tree(path, src, lines, tree, issues):
    root = tree.root_node
    lint_includes(path, src, root, issues)
    for node in walk(root):
        fn = CHECKS.get(node.type)
        if fn:
            fn(src, node, lines, issues, path)


def lint_file(path):
    raw = Path(path).read_bytes()
    lines = raw.decode("utf-8", "replace").splitlines()
    tree = PARSER.parse(raw)
    issues = []
    lint_tree(str(path), raw, lines, tree, issues)
    return issues


def main():
    ap = argparse.ArgumentParser(description="tree-sitter C style linter")
    ap.add_argument("paths", nargs="+")
    args = ap.parse_args()
    paths = collect_paths(args.paths)
    if not paths:
        print("no C files", file=sys.stderr)
        return 2
    n = 0
    for p in paths:
        for it in lint_file(p):
            print(it)
            n += 1
    if n:
        print("%d issue(s) in %d file(s)" % (n, len(paths)))
        return 1
    print("ok (%d file(s))" % len(paths))
    return 0


if __name__ == "__main__":
    sys.exit(main())
