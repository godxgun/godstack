#!/usr/bin/env python3
"""Lint demo sources and measure function complexity without Python dependencies.

C/Slang analysis is lexical: comments, strings and preprocessor directives are
ignored; all conditional-compilation branches are counted together. It does not
replace a compiler. Python functions are analyzed using the standard-library AST.
"""

import argparse
import ast
from dataclasses import asdict, dataclass
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tokenize

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".h", ".slang", ".py", ".sh"}
EXCLUSIONS = {
    "teapot/stb_image.h": "vendored stb_image implementation",
    "teapot/teapot.h": "generated mesh data",
    "doc-generator/view.cool.c": "generated Cool template output",
    "grit/workload.c": "verbatim historical workload, protected by workload.sha256",
}
TOKEN = re.compile(r"[A-Za-z_]\w*|&&|\|\||[^\s]", re.ASCII)
C_NON_CODE = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
    re.DOTALL,
)


@dataclass
class Function:
    path: str
    line: int
    name: str
    complexity: int
    lines: int


def blank(match):
    return "".join("\n" if char == "\n" else " " for char in match.group())


def c_code(source):
    code = C_NON_CODE.sub(blank, source)
    return re.sub(r"^[ \t]*#(?:[^\n\\]|\\[^\n]|\\\n)*", blank, code, flags=re.MULTILINE)


def c_functions(source, path):
    code = c_code(source)
    tokens = list(TOKEN.finditer(code))
    depth = 0
    boundary = 0
    opening = None
    name = None
    functions = []
    for index, token in enumerate(tokens):
        value = token.group()
        if value == "{":
            if depth == 0:
                header = tokens[boundary:index]
                # A top-level parameter list identifies a definition; array
                # attributes and declarations/initializers are not functions.
                parens = brackets = 0
                name = None
                for offset, item in enumerate(header):
                    text = item.group()
                    if text == "[":
                        brackets += 1
                    elif text == "]":
                        brackets -= 1
                    elif text == "(" and not brackets:
                        if not parens and offset:
                            candidate = header[offset - 1].group()
                            if re.fullmatch(r"[A-Za-z_]\w*", candidate) and candidate not in {
                                "__attribute__", "__declspec", "alignas", "sizeof",
                            }:
                                name = candidate
                        parens += 1
                    elif text == ")" and not brackets:
                        parens -= 1
                    elif text == "=" and not parens and not brackets:
                        name = None
                        break
                opening = index
            depth += 1
        elif value == "}":
            depth -= 1
            if depth == 0:
                if name is not None:
                    body = tokens[opening + 1:index]
                    decisions = sum(item.group() in {
                        "if", "for", "while", "case", "catch", "&&", "||", "?",
                    } for item in body)
                    start = tokens[boundary].start()
                    end = token.end()
                    functions.append(Function(
                        path, code.count("\n", 0, start) + 1, name, 1 + decisions,
                        sum(bool(line.strip()) for line in code[start:end].splitlines()),
                    ))
                boundary = index + 1
                name = None
        elif value == ";" and depth == 0:
            boundary = index + 1
    return functions


class PythonDecisions(ast.NodeVisitor):
    def __init__(self):
        self.count = 0

    def generic_visit(self, node):
        if isinstance(node, (ast.If, ast.IfExp, ast.For, ast.AsyncFor, ast.While,
                             ast.ExceptHandler)):
            self.count += 1
        elif isinstance(node, ast.BoolOp):
            self.count += len(node.values) - 1
        elif isinstance(node, ast.comprehension):
            self.count += 1 + len(node.ifs)
        elif isinstance(node, ast.match_case):
            # A wildcard case is the default path, unless it has a guard.
            if not isinstance(node.pattern, ast.MatchAs) or node.pattern.pattern is not None:
                self.count += 1
            if node.guard is not None:
                self.count += 1
        super().generic_visit(node)

    def visit_FunctionDef(self, node):
        pass  # Nested functions are measured separately.

    visit_AsyncFunctionDef = visit_FunctionDef
    visit_Lambda = visit_FunctionDef
    visit_ClassDef = visit_FunctionDef


def python_functions(source, path):
    tree = ast.parse(source, filename=path)
    functions = []
    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        visitor = PythonDecisions()
        for statement in node.body:
            visitor.visit(statement)
        functions.append(Function(path, node.lineno, node.name, visitor.count + 1,
                                  node.end_lineno - node.lineno + 1))
    return functions


def lint_source(source, path, suffix, max_line_length):
    issues = []
    if source and not source.endswith("\n"):
        issues.append(f"{path}: missing final newline")
    if "\r" in source:
        issues.append(f"{path}: use LF line endings")
    code = source
    if suffix in {".c", ".h", ".slang"}:
        # Preserve line numbers, but don't charge long literal contents or
        # comments against the code width limit.
        code = C_NON_CODE.sub(lambda match: '""' + "\n" * match.group().count("\n"), source)
    if suffix == ".py":
        # Long literal strings can be clearer without artificial splitting.
        spans = []
        for token in tokenize.generate_tokens(io.StringIO(source).readline):
            if token.type in {tokenize.STRING, tokenize.COMMENT}:
                spans.append((token.start, token.end))
        lines = [list(line) for line in source.splitlines()]
        for (start_line, start_col), (end_line, end_col) in reversed(spans):
            for row in range(start_line - 1, end_line):
                first = start_col if row == start_line - 1 else 0
                last = end_col if row == end_line - 1 else len(lines[row])
                lines[row][first:last] = '""'
        code = "\n".join("".join(line) for line in lines)
    for line, (original, content) in enumerate(zip(source.splitlines(), code.splitlines()), 1):
        if original.rstrip() != original:
            issues.append(f"{path}:{line}: trailing whitespace")
        if len(content.rstrip().expandtabs(4)) > max_line_length:
            issues.append(f"{path}:{line}: code exceeds {max_line_length} columns")
    return issues


def source_files(demos):
    return sorted(path for path in demos.rglob("*") if path.is_file()
                  and path.suffix in SOURCE_SUFFIXES
                  and not any(part.startswith(".") or part == "__pycache__"
                              for part in path.relative_to(demos).parts)
                  and path.relative_to(demos).as_posix() not in EXCLUSIONS)


def compile_source(path, compiler):
    """Check the native C profile; only demo-owned warnings fail the audit."""
    extra = []
    if path.name == "shaping.c":
        extra = subprocess.run(["pkg-config", "--cflags", "harfbuzz", "fribidi"],
                               check=True, capture_output=True, text=True).stdout.split()
    if path.parent.name in {"compute", "dashboard", "teapot", "rend"}:
        extra.append("-DPEAK_VULKAN")
    result = subprocess.run([
        compiler, "-std=c99", "-fsyntax-only", "-I", str(ROOT), "-Wall", "-Wextra",
        "-Wno-deprecated-declarations", *extra, str(path),
    ], capture_output=True, text=True)
    diagnostics = []
    for line in result.stderr.splitlines():
        match = re.match(r"(.+?):\d+:\d+: warning:", line)
        if match:
            owner = Path(match.group(1)).resolve()
            if owner.is_relative_to(ROOT / "demos"):
                relative = owner.relative_to(ROOT / "demos").as_posix()
                if relative not in EXCLUSIONS:
                    diagnostics.append(line)
    diagnostics.extend(line for line in result.stderr.splitlines() if "error:" in line)
    if result.returncode and not diagnostics:
        diagnostics.append(result.stderr.strip() or f"compiler exited {result.returncode}")
    return diagnostics


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--max-complexity", type=int, default=20)
    parser.add_argument("--max-function-lines", type=int, default=160)
    parser.add_argument("--max-line-length", type=int, default=120)
    parser.add_argument("--report", type=Path, help="write full metrics as JSON")
    parser.add_argument("--top", type=int, default=10, help="number of largest functions to show")
    parser.add_argument("--compile", action="store_true",
                        help="also check C syntax/warnings with Clang (native SDKs required)")
    formatting = parser.add_mutually_exclusive_group()
    formatting.add_argument("--check-format", action="store_true", help="also check C/headers with clang-format")
    formatting.add_argument("--format", action="store_true", help="format C/headers with clang-format, then lint")
    args = parser.parse_args(argv)
    if min(args.max_complexity, args.max_function_lines, args.max_line_length) < 1 or args.top < 0:
        parser.error("limits must be positive and --top nonnegative")
    formatter = shutil.which("clang-format") if args.format or args.check_format else None
    if (args.format or args.check_format) and not formatter:
        parser.error("clang-format is required for formatting")
    compiler = shutil.which("clang") if args.compile else None
    if args.compile and not compiler:
        parser.error("clang is required for --compile")
    files = source_files(ROOT / "demos")
    functions, issues = [], []
    for path in files:
        display = path.relative_to(ROOT).as_posix()
        try:
            source = path.read_text(encoding="utf-8")
            if formatter and path.suffix in {".c", ".h"}:
                formatted = subprocess.run([formatter, str(path)], check=True,
                                           capture_output=True, text=True).stdout
                if source != formatted:
                    if args.format:
                        path.write_text(formatted, encoding="utf-8")
                        source = formatted
                    else:
                        issues.append(f"{display}: clang-format differs (run --format)")
            issues.extend(lint_source(source, display, path.suffix, args.max_line_length))
            if path.suffix == ".py":
                functions.extend(python_functions(source, display))
            elif path.suffix in {".c", ".h", ".slang"}:
                functions.extend(c_functions(source, display))
            elif path.suffix == ".sh":
                result = subprocess.run(["bash", "-n", str(path)], capture_output=True, text=True)
                if result.returncode:
                    issues.append(f"{display}: shell syntax error: {result.stderr.strip()}")
            if compiler and path.suffix == ".c":
                issues.extend(compile_source(path, compiler))
        except (OSError, UnicodeError, SyntaxError, tokenize.TokenError,
                subprocess.CalledProcessError) as error:
            issues.append(f"{display}: {error}")
    functions.sort(key=lambda function: (-function.complexity, -function.lines,
                                         function.path, function.line))
    for function in functions:
        location = f"{function.path}:{function.line}: {function.name}"
        if function.complexity > args.max_complexity:
            issues.append(f"{location}: complexity {function.complexity} > {args.max_complexity}")
        if function.lines > args.max_function_lines:
            issues.append(f"{location}: function lines {function.lines} > {args.max_function_lines}")
    if args.report:
        args.report.write_text(json.dumps({
            "files": len(files), "functions": [asdict(function) for function in functions],
            "exclusions": EXCLUSIONS, "issues": issues,
        }, indent=2) + "\n", encoding="utf-8")
    for function in functions[:args.top]:
        print(f"CC {function.complexity:2} / {function.lines:3} lines  "
              f"{function.path}:{function.line} {function.name}")
    for issue in issues:
        print(issue, file=sys.stderr)
    print(f"Demo lint: {len(files)} files, {len(functions)} functions, "
          f"{len(EXCLUSIONS)} explicit exclusions, {len(issues)} issues")
    return int(bool(issues))


if __name__ == "__main__":
    sys.exit(main())
