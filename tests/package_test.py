#!/usr/bin/env python3
"""Host package consumer check: no GPU, display, or source-tree headers."""
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
    if sys.platform == "win32":
        libraries = ["-lvulkan-1", "-luser32", "-lgdi32", "-lshell32", "-lole32",
                     "-luuid", "-lwinmm", "-lws2_32", "-ladvapi32", "-lcomdlg32",
                     "-ldwmapi", "-lshcore", "-lm", "-lz"]
    elif sys.platform == "darwin":
        libraries = ["-lvulkan", "-lm", "-lz"]
        for framework in ("AppKit", "AudioToolbox", "QuartzCore", "CoreGraphics",
                          "GameController"):
            libraries.extend(["-framework", framework])
    elif sys.platform.startswith("linux"):
        libraries = ["-lvulkan", "-ldl", "-lpthread", "-lutil", "-lm", "-lz"]
    else:
        raise SystemExit(f"Unsupported package test host: {sys.platform}")
    build = ROOT / ("build.exe" if sys.platform == "win32" else "build")
    result = subprocess.run([str(build), "package"], cwd=ROOT, check=True,
                            text=True, stdout=subprocess.PIPE)
    print(result.stdout, end="")
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    ready = re.search(r"^Package ready: (.+)$", result.stdout, re.M)
    assert ready, "Build did not report a completed package"
    package = ROOT / ready.group(1).strip()
    # Use the reported date to avoid crossing midnight between build and test.
    date = package.name.removesuffix(f"-{commit}")
    datetime.date.fromisoformat(date)
    assert package.name == f"{date}-{commit}"
    assert package.parent == ROOT / "package"
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
        obj = (package / "bin" / name).read_bytes()
        if sys.platform.startswith("linux"):
            assert obj[:4] == b"\x7fELF"
            assert int.from_bytes(obj[16:18], "little" if obj[5] == 1 else "big") == 1
        elif sys.platform == "darwin":
            assert obj[:4] in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe")
            assert int.from_bytes(obj[12:16], "little") == 1  # MH_OBJECT
        else:
            assert obj[:2] in (b"\x64\x86", b"\x4c\x01", b"\x64\xaa")  # COFF
            assert int.from_bytes(obj[16:18], "little") == 0  # no optional header
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
        executable = relocated / "bin" / ("consumer.exe" if sys.platform == "win32" else "consumer")
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", str(source),
                        "-I", str(relocated / "include"),
                        *[str(relocated / "bin" / f"{n}-{v}.o")
                          for n, v in versions.items()],
                        *libraries,
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
