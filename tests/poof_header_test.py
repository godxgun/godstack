#!/usr/bin/env python3
"""Check standalone Poof header inclusion, static linkage and self-rebuilding."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = os.environ.get("CC", "gcc")
    suffix = ".exe" if sys.platform == "win32" else ""
    with tempfile.TemporaryDirectory(prefix="poof-header-") as tmp:
        work = Path(tmp)
        shutil.copyfile(ROOT / "Poof" / "poof.h", work / "poof.h")
        (work / "main.c").write_text('''#include "poof.h"
#include "poof.h"
int other(void);
int main(void)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "main");
    int ok = cmd.count == 1 && other();
    poof_cmd_free(&cmd);
    return ok ? 0 : 1;
}
''')
        (work / "other.c").write_text('''#include "poof.h"
int other(void)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "other");
    int ok = cmd.count == 1;
    poof_cmd_free(&cmd);
    return ok;
}
''')
        executable = work / f"consumer{suffix}"
        subprocess.run([compiler, "-std=c99", "-Werror=implicit-function-declaration",
                        "main.c", "other.c", "-o", str(executable)], cwd=work, check=True)
        subprocess.run([str(executable)], cwd=work, check=True)

        # Only the header is newer than the binary, so this exercises header tracking.
        source = work / "build.c"
        source.write_text('''#include "poof.h"
int main(int argc, char **argv)
{
    POOF_GO_REBUILD_URSELF(argc, argv);
    puts("standalone rebuild: PASS");
    return 0;
}
''')
        executable = work / f"build{suffix}"
        subprocess.run([compiler, "-std=c99", str(source), "-o", str(executable)],
                       cwd=work, check=True)
        os.utime(source, (1, 1))
        os.utime(executable, (1, 1))
        result = subprocess.run([str(executable)], cwd=work, text=True,
                                capture_output=True, check=True, timeout=60)
        assert "Rebuild successful!" in result.stdout, result.stdout + result.stderr
        assert "standalone rebuild: PASS" in result.stdout
        result = subprocess.run([str(executable)], cwd=work, text=True,
                                capture_output=True, check=True, timeout=60)
        assert "Rebuilding" not in result.stdout, result.stdout + result.stderr
    print("Poof standalone header, repeated inclusion, multi-TU linkage and rebuild: PASS")


if __name__ == "__main__":
    main()
