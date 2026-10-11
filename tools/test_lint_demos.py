"""Parser and failure-mode checks for the demo audit."""

import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import lint_demos as lint


class DemoLintTests(unittest.TestCase):
    def test_c_comments_strings_and_macros_are_not_decisions(self):
        source = r'''
#define CHECK(x) do { \
    if (!(x)) return 0; \
} while (0)
/* if (x) { while (y) {} } */
static int example(int a) {
    const char *text = "if for while ? && || { }";
    char brace = '}';
    if (a && a > 1) return 2;
    return a ? 1 : 0;
}
'''
        functions = lint.c_functions(source, "example.c")
        self.assertEqual([(f.name, f.complexity) for f in functions], [("example", 4)])

    def test_c_prototypes_initializers_and_attribute_functions(self):
        source = '''
int prototype(int);
struct Item { int (*callback)(int); };
struct Item data = { make_callback(3) };
__attribute__((unused)) static void work(void) {
    for (int i = 0; i < 10; i++) if (i) continue;
}
'''
        self.assertEqual([(f.name, f.complexity) for f in lint.c_functions(source, "test.c")],
                         [("work", 3)])

    def test_shader_semantics_and_attributes(self):
        source = '''[shader("vertex")]
float4 vertexMain(float3 position : POSITION) : SV_Position {
    return position.x > 0 ? float4(position, 1) : float4(0);
}
'''
        self.assertEqual([(f.name, f.complexity) for f in lint.c_functions(source, "test.slang")],
                         [("vertexMain", 2)])

    def test_python_nested_functions_and_comprehensions(self):
        source = '''def outer(values):
    def inner(x):
        if x or x == 1: return 1
    return [inner(x) for x in values if x]
'''
        self.assertEqual({f.name: f.complexity for f in lint.python_functions(source, "test.py")},
                         {"outer": 3, "inner": 3})

    def test_hygiene_and_literal_lengths(self):
        source = 'int f(void) {  \n    puts("' + 'a' * 200 + '");\n    return 0;\n}'
        issues = lint.lint_source(source, "test.c", ".c", 120)
        self.assertEqual(len(issues), 2)
        self.assertIn("missing final newline", issues[0])
        self.assertIn("trailing whitespace", issues[1])

    def test_exclusions_and_hidden_generated_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            demos = Path(directory)
            for name in ("example.c", "teapot/stb_image.h", ".checks/generated.c", "binary.wasm"):
                path = demos / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("int main(void) { return 0; }\n")
            self.assertEqual([p.name for p in lint.source_files(demos)], ["example.c"])

    def test_cli_fails_on_syntax_and_complexity_and_writes_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "demos").mkdir()
            (root / "demos" / "bad.py").write_text("def invalid(:\n")
            (root / "demos" / "complex.c").write_text("int f(int n) { if(n) return 1; return 0; }\n")
            report = root / "report.json"
            with patch.object(lint, "ROOT", root), contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()):
                result = lint.main(["--max-complexity", "1", "--report", str(report)])
            self.assertEqual(result, 1)
            self.assertIn('"complexity": 2', report.read_text())
            self.assertIn("demos/bad.py:", report.read_text())

    def test_cli_clean_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "demos").mkdir()
            (root / "demos" / "good.c").write_text("int main(void) { return 0; }\n")
            with patch.object(lint, "ROOT", root), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main([]), 0)


if __name__ == "__main__":
    unittest.main()
