#!/usr/bin/env python3
"""Linux package consumer check: no GPU, display, or source-tree headers."""
import datetime
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not sys.platform.startswith("linux"):
        raise SystemExit("This relocation check currently requires Linux")
    subprocess.run(["./build", "package"], cwd=ROOT, check=True)
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    package = ROOT / "package" / f"{datetime.date.today().isoformat()}-{commit}"
    versions = {}
    for directory in ("Peak", "Fuse", "Rend", "Type"):
        name = directory.lower()
        header = (ROOT / directory / f"{name}.h").read_text()
        parts = [re.search(rf'^#define {directory.upper()}_{part}\s+"?(\d+)',
                           header, re.M).group(1)
                 for part in ("MAJOR", "MINOR", "PATCH")]
        versions[name] = ".".join(parts)
    expected_headers = {f"{n}-{v}.h" for n, v in versions.items()}
    expected_libraries = {f"{n}-{v}.o" for n, v in versions.items()}
    assert expected_headers <= {p.name for p in (package / "include").iterdir()}
    assert expected_libraries == {p.name for p in (package / "bin").iterdir()}
    for name in expected_libraries:
        elf = (package / "bin" / name).read_bytes()
        assert elf[:4] == b"\x7fELF"
        assert int.from_bytes(elf[16:18], "little" if elf[5] == 1 else "big") == 1
    assert (package / "LICENSE").is_file()
    with tempfile.TemporaryDirectory(prefix="godstack-package-") as tmp:
        relocated = Path(tmp) / "relocated"
        shutil.copytree(package, relocated)
        source = Path(tmp) / "consumer.c"
        source.write_text(
            "".join(f'#include "{n}-{v}.h"\n' for n, v in versions.items())
            + '''int main(void)
{
    TypeParams params;
    type_params_default(&params);
    if (!type_memory(&params)) return 1;
    if (!fuse_canvas_memory(16)) return 2;
    if (!peak_get_time()) return 3;
    /* Reference Rend's exported API without creating a GPU context. */
    void (*volatile render_end)(RendRenderer) = rend_cmd_render_end;
    return render_end ? 0 : 4;
}
''')
        executable = relocated / "bin" / "consumer"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", str(source),
                        "-I", str(relocated / "include"),
                        *[str(relocated / "bin" / f"{n}-{v}.o")
                          for n, v in versions.items()],
                        "-lvulkan", "-ldl", "-lpthread", "-lutil", "-lm", "-lz",
                        "-o", str(executable)], check=True)
        # The executable must run without the packaged objects at runtime.
        for obj in (relocated / "bin").glob("*.o"):
            obj.unlink()
        env = os.environ.copy()
        env.pop("LD_LIBRARY_PATH", None)
        subprocess.run([str(executable)], cwd=tmp, env=env, check=True)
    print("Package naming, headers, static-object linking and relocation: PASS")


if __name__ == "__main__":
    main()
