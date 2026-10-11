"""Run with: python3 -m unittest discover -s tools -p 'test_*.py'."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import generate_docs as docs


def header(body, library="Cast"):
    return (f"/* Unboxing example */\n#if 0\nvoid cast_example(void);\n#endif\n"
            f"#ifndef {library.upper()}_H\n#define {library.upper()}_H\n"
            + body + f"\n#endif\n#ifdef {library.upper()}_IMPLEMENTATION\n"
            "static void cast_private(void) {}\n#endif\n")


class ExtractionTests(unittest.TestCase):
    def test_multiline_doxygen_and_line_numbers(self):
        source = header("""/**
 * @brief Do something.
 * @param[in] value Input value.
 * @return Zero on success.
 */
extern int cast_do(
    int value,
    void (*callback)(int));
""")
        entries = docs.extract(source, "Cast")
        self.assertEqual([e.name for e in entries], ["cast_do"])
        entry = entries[0]
        self.assertEqual(entry.line, source[:source.index("extern int")].count("\n") + 1)
        self.assertIn("void (*callback)(int)", entry.declaration)
        self.assertIn("Do something.", entry.description)
        self.assertIn("**`value` (in):** Input value.", entry.description)
        self.assertIn("**Returns:** Zero on success.", entry.description)

    def test_types_and_trailing_comments(self):
        entries = docs.extract(header("""/** Aggregate documentation. */
typedef struct {
    int value; /* member documentation */
    const char *text;
} CastThing; ///< Trailing documentation.
typedef void (*CastCallback)(CastThing *);
void cast_send(const char *url); // Accepts https://example.org.
"""), "Cast")
        self.assertEqual([e.name for e in entries], ["CastThing", "CastCallback", "cast_send"])
        self.assertIn("member documentation", entries[0].declaration)
        self.assertIn("Aggregate documentation.", entries[0].description)
        self.assertIn("Trailing documentation.", entries[0].description)
        self.assertEqual(entries[2].description, "Accepts https://example.org.")

    def test_indented_directives_and_continued_macro(self):
        source = header("""#if defined(PLATFORM)
    # include <platform.h>
    # define CAST_CALL(x) \\
        cast_call(x)
#else
    #define CAST_CALL(x) cast_other(x)
#endif
enum CastKind { CAST_LEFT = '{', CAST_RIGHT = '}' };
void cast_call(int value);
""")
        entries = docs.extract(source, "Cast")
        self.assertEqual([e.name for e in entries], ["CAST_CALL", "CAST_CALL", "CastKind", "cast_call"])
        self.assertEqual(entries[2].kind, "Types")
        self.assertIn("cast_call(x)", entries[0].declaration)

    def test_rend2_prefix_and_poof_boundary(self):
        self.assertEqual(docs.extract(header("int rend_init(void);", "Rend2"), "Rend2")[0].name, "rend_init")
        source = "static void poof_run(void);\n/* Implementation */\nstatic void poof_private(void) {}"
        self.assertEqual([e.name for e in docs.extract(source, "Poof")], ["poof_run"])

    def test_repository_libraries_have_types_and_functions(self):
        for library in docs.LIBRARIES:
            with self.subTest(library=library):
                entries = docs.extract((docs.ROOT / f"{library}.h").read_text(), library)
                self.assertTrue(any(e.kind == "Types" for e in entries))
                self.assertTrue(any(e.kind == "Functions" for e in entries))
                self.assertFalse(any("cast_private" == e.name for e in entries))


class PageTests(unittest.TestCase):
    def test_preserves_manual_content_and_is_idempotent(self):
        existing = "+++\ntitle = 'Custom'\n+++\n\nManual prose.\n"
        section = docs.generated_section("Cast", [docs.Entry("cast_do", "Functions", "void cast_do(void);", "Useful.", 1)])
        page = docs.update_page(existing, "Cast", section)
        self.assertTrue(page.startswith(existing))
        page += "\nManual footer.\n"
        updated = docs.update_page(page, "Cast", section.replace("Useful.", "Changed."))
        self.assertTrue(updated.endswith("\nManual footer.\n"))
        self.assertEqual(docs.update_page(updated, "Cast", section.replace("Useful.", "Changed.")), updated)
        self.assertEqual(updated.count(docs.BEGIN), 1)

    def test_refuses_broken_markers(self):
        for page in (docs.BEGIN, docs.END, docs.END + docs.BEGIN, docs.BEGIN * 2 + docs.END):
            with self.subTest(page=page), self.assertRaises(ValueError):
                docs.update_page(page, "Cast", "replacement")

    def test_check_reports_staleness_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Cast.h").write_text(header("void cast_run(void);"))
            with patch.object(docs, "ROOT", root), patch.object(docs, "LIBRARIES", ("Cast",)):
                with patch.object(sys, "argv", ["generate_docs", "--check"]):
                    self.assertEqual(docs.main(), 1)
                    self.assertFalse((root / "site").exists())
                with patch.object(sys, "argv", ["generate_docs"]):
                    self.assertEqual(docs.main(), 0)
                page = root / "site/articles/symbols/cast.md"
                before = page.read_bytes()
                with patch.object(sys, "argv", ["generate_docs", "--check"]):
                    self.assertEqual(docs.main(), 0)
                    self.assertEqual(page.read_bytes(), before)
                (root / "Cast.h").write_text(header("void cast_changed(void);"))
                with patch.object(sys, "argv", ["generate_docs", "--check"]):
                    self.assertEqual(docs.main(), 1)
                    self.assertEqual(page.read_bytes(), before)

    def test_cli_works_outside_repository(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([sys.executable, str(docs.ROOT / "tools/generate_docs.py"), "--check"],
                                    cwd=directory, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
