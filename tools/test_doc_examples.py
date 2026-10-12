"""Compile the public CPU-only examples; never run the Wire network server."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from site_paths import SITE

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(SITE.is_dir(), "external website checkout unavailable (set GODSTACK_SITE)")
@unittest.skipUnless(shutil.which("cc"), "C compiler not available")
class DocumentationExamples(unittest.TestCase):
    def test_examples_match_current_headers(self):
        examples = {
            "cast": ("tokens: 3", []),
            "cool": ("Hello &amp; welcome", []),
            "fuse": ("", ["-lm"]),
            "poof": ("", []),
            "type": ("", ["-lm"]),
            "wire": (None, []),  # compile-only: would bind a public network port
        }
        with tempfile.TemporaryDirectory(prefix="godstack-doc-examples-") as directory:
            directory = Path(directory)
            for name, (expected, libraries) in examples.items():
                with self.subTest(library=name):
                    body = (SITE / f"articles/docs/{name}.md").read_text().split("<!-- BEGIN GENERATED API -->")[0]
                    block = re.search(r"```c\n(.*?)\n```", body, re.DOTALL)
                    self.assertIsNotNone(block, f"{name} must have a C example")
                    source = directory / f"{name}.c"
                    source.write_text(block[1] + "\n")
                    binary = directory / name
                    flags = ["-c"] if expected is None else []
                    result = subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-I", str(ROOT),
                                             *flags, str(source), *libraries, "-o", str(binary)],
                                            capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if expected is not None:
                        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn(expected, result.stdout)

    def test_getting_started_example(self):
        body = (SITE / "articles/guides/getting-started.md").read_text()
        code = re.search(r"<<'EOF'\n(.*?)\nEOF", body, re.DOTALL)
        self.assertIsNotNone(code, "getting-started must supply a complete C program")
        with tempfile.TemporaryDirectory(prefix="godstack-start-example-") as directory:
            source = Path(directory) / "hello.c"
            binary = Path(directory) / "hello"
            source.write_text(code[1] + "\n")
            result = subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-I", str(ROOT), str(source),
                                     "-o", str(binary)], capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), "value: 42")


if __name__ == "__main__":
    unittest.main()
