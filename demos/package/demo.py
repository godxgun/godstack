#!/usr/bin/env python3
"""Compile copied single headers as objects and link a relocated CPU consumer.

Everything is built in a temporary directory; no distribution/build-driver
feature or source-tree include path is required by the consumer.
"""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LIBRARIES = ("Peak", "Fuse", "Rend", "Type")
CONSUMER = '''#include "Poof.h"
#include "Peak.h"
#include "Fuse.h"
#include "Rend.h"
#include "Type.h"
int main(void)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "package-consumer");
    if (cmd.count != 1) return 5;
    poof_cmd_free(&cmd);
    TypeParams params;
    type_params_default(&params);
    if (!type_memory(&params)) return 1;
    if (!fuse_canvas_memory(16)) return 2;
    if (!peak_get_time()) return 3;
    /* Reference the exported API without creating a GPU context. */
    void (*volatile render_end)(RendRenderer) = rend_cmd_render_end;
    return render_end ? 0 : 4;
}
'''


def host_libraries():
    if sys.platform == "win32":
        return ["-lvulkan-1", "-luser32", "-lgdi32", "-lshell32", "-lole32",
                "-luuid", "-lwinmm", "-lws2_32", "-ladvapi32", "-lcomdlg32",
                "-ldwmapi", "-lshcore", "-lm", "-lz"]
    if sys.platform == "darwin":
        libraries = ["-lvulkan", "-lm", "-lz"]
        for framework in ("AppKit", "AudioToolbox", "QuartzCore", "CoreGraphics", "GameController"):
            libraries.extend(["-framework", framework])
        return libraries
    if sys.platform.startswith("linux"):
        return ["-lvulkan", "-ldl", "-lpthread", "-lutil", "-lm", "-lz"]
    raise RuntimeError(f"unsupported consumer host: {sys.platform}")


def compile_objects(package, compiler):
    objects = []
    for name in LIBRARIES:
        source = package / f"{name}.c"
        source.write_text(f'#define {name.upper()}_IMPLEMENTATION\n#include "{name}.h"\n')
        obj = package / "bin" / f"{name}.o"
        subprocess.run([*compiler, "-c", str(source), "-I", str(package / "include"),
                        "-o", str(obj)], cwd=package, check=True)
        objects.append(obj)
    return objects


def main():
    libraries = host_libraries()
    compiler = [os.environ.get("CC", "cc"), "-std=c99", "-O2",
                "-Wno-deprecated-declarations", "-DPEAK_NO_AUDIO", "-DPEAK_NO_GAMEPAD"]
    if sys.platform != "darwin":
        compiler.append("-DPEAK_VULKAN")
    with tempfile.TemporaryDirectory(prefix="godstack-consumer-") as tmp:
        package = Path(tmp) / "package"
        (package / "include").mkdir(parents=True)
        (package / "bin").mkdir()
        for name in (*LIBRARIES, "Poof"):
            shutil.copy2(ROOT / f"{name}.h", package / "include")
        for notice in ("LICENSE", "THIRD-PARTY-NOTICES.md"):
            shutil.copy2(ROOT / notice, package)
        compile_objects(package, compiler)
        relocated = Path(tmp) / "relocated"
        shutil.move(str(package), relocated)
        source = relocated / "consumer.c"
        source.write_text(CONSUMER)
        executable = relocated / "bin" / ("consumer.exe" if sys.platform == "win32" else "consumer")
        objects = [relocated / "bin" / f"{name}.o" for name in LIBRARIES]
        subprocess.run([*compiler, str(source), "-I", str(relocated / "include"),
                        *map(str, objects), *libraries, "-o", str(executable)],
                       cwd=relocated, check=True)
        for obj in objects:
            obj.unlink()
        subprocess.run([str(executable)], cwd=relocated, check=True)
    print("Copied single headers, static-object linking and relocation: PASS")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"package consumer: {error}", file=sys.stderr)
        sys.exit(1)
