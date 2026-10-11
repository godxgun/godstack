#!/usr/bin/env python3
"""Build native Peak visual demos as single-file WebAssembly artifacts.

The same C entry point is used by native builds and Emscripten. Peak owns the
window, input events, backbuffer and presentation; the HTML runner only creates
the module and forwards the browser's reduced-motion preference at startup.
Requires Emscripten (emcc). Uses an isolated cache and publishes only after all
requested demos have compiled. --check verifies published fingerprints.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEMOS = {"grit": "Grit.h", "snake": "Rend.h"}
TITLES = {"grit": "Particle Observatory", "snake": "Snake"}
FLAGS = [
    "-std=c99", "-O2", "-Wall", "-Wextra",
    "-sMODULARIZE=1", "-sEXPORT_NAME=createDemo", "-sENVIRONMENT=web",
    "-sSINGLE_FILE=1", "-sEXIT_RUNTIME=0", "-sALLOW_MEMORY_GROWTH=1",
    "-sSTACK_SIZE=1048576", "-sEXPORTED_FUNCTIONS=['_main']",
]
SHELL = '''<!doctype html>
<html lang="en" data-run-state="loading">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>__TITLE__</title>
<style>
*{box-sizing:border-box}html,body,main{width:100%;height:100%;margin:0;overflow:hidden;background:transparent}
main{display:grid;place-items:center}
canvas{display:block;width:100%;height:100%;min-width:0;min-height:0;outline:none}
canvas:focus-visible{outline:2px solid #ff8800;outline-offset:-2px}
#status{position:fixed;inset:0;display:grid;place-items:center;margin:0;padding:1rem;background:#141413;color:#eeedea;font:15px/1.65 Inter,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
#status[hidden]{display:none}
</style></head>
<body><main><canvas id="canvas" width="960" height="720" tabindex="0" aria-label="__CONTROLS__"></canvas></main>
<p id="status" role="status">Loading __TITLE__…</p><noscript>This demo requires JavaScript and WebAssembly.</noscript>
<script src="demo.js"></script><script>
(()=>{const status=document.getElementById('status'),root=document.documentElement;
function failed(error){root.dataset.runState='failed';status.hidden=false;status.textContent='__TITLE__ failed: '+String(error);}
function ready(){if(root.dataset.runState!=='failed'){root.dataset.runState='ready';status.hidden=true;}}
try{
  if(typeof WebAssembly==='undefined')throw new Error('This browser does not support WebAssembly');
  if(typeof createDemo!=='function')throw new Error('Compiled __TITLE__ assets are unavailable');
  createDemo({arguments:__ARGUMENTS__,onRuntimeInitialized:ready,onAbort:failed,printErr:failed}).then(()=>{
    ready();
    const canvas=document.getElementById('canvas'),stage=document.querySelector('main');
    function fit(){const w=Math.max(1,Math.round(stage.clientWidth)),h=Math.max(1,Math.round(stage.clientHeight));if(canvas.width!==w)canvas.width=w;if(canvas.height!==h)canvas.height=h;}
    new ResizeObserver(fit).observe(stage);fit();
  }).catch(failed);
}catch(error){failed(error);}
})();
</script></body></html>
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def demo_flags(name):
    return FLAGS + (["-sASYNCIFY=1", "-sMIN_WEBGL_VERSION=2", "-sMAX_WEBGL_VERSION=2"] if name == "snake" else [])


def source_path(name):
    return ROOT / f"demos/{name}/{'snake' if name == 'snake' else 'demo'}.c"


def input_hash(name):
    inputs = [source_path(name), ROOT / DEMOS[name], ROOT / "Peak.h", Path(__file__)]
    inputs += ([ROOT / "Grit.h"] if name == "snake" else [ROOT / f"demos/{name}/visual.h"])
    return digest(json.dumps(demo_flags(name)).encode() + b"".join(path.read_bytes() for path in inputs))


def current(destination, name):
    directory = destination / name
    try:
        manifest = json.loads((directory / "manifest.json").read_text())
        return (manifest["input_hash"] == input_hash(name)
                and all(digest((directory / filename).read_bytes()) == checksum
                        for filename, checksum in manifest["files"].items())
                and {"demo.js", "run.html", "LICENSE.txt"}.issubset(manifest["files"]))
    except (OSError, KeyError, ValueError):
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("demos", nargs="*", choices=tuple(DEMOS), help="default: both demos")
    parser.add_argument("--check", action="store_true", help="check sources and artifacts without compiling")
    parser.add_argument("--output", type=Path, default=ROOT / "site/static/demos")
    args = parser.parse_args()
    names = args.demos or list(DEMOS)
    if args.check:
        stale = [name for name in names if not current(args.output, name)]
        for name in stale:
            print(f"stale web demo: {name}")
        return int(bool(stale))

    compiler = shutil.which("emcc")
    if not compiler:
        print("emcc not found. Install Emscripten separately, or deploy the checked-in static artifacts.", file=sys.stderr)
        return 1
    try:
        version = subprocess.check_output([compiler, "--version"], text=True).splitlines()[0]
        with tempfile.TemporaryDirectory(prefix="godstack-web-demos-") as temporary:
            temporary = Path(temporary)
            cache = temporary / "cache"
            existing_cache = Path(os.environ.get("EM_CACHE", Path.home() / ".cache/emscripten"))
            if existing_cache.is_dir():
                shutil.copytree(existing_cache, cache)
            else:
                cache.mkdir()
            environment = dict(os.environ, EM_CACHE=str(cache))
            staging = temporary / "output"
            for name in names:
                directory = staging / name
                directory.mkdir(parents=True)
                source = source_path(name)
                if not source.is_file():
                    raise ValueError(f"missing shared native/Peak source: {source}")
                command = [compiler, *demo_flags(name), "-I", str(ROOT),
                           f"-ffile-prefix-map={ROOT}=.", f"-ffile-prefix-map={temporary}=.",
                           str(source), "-lm", "-o", str(directory / "demo.js")]
                print(f"building shared Peak C demo {source.relative_to(ROOT)}", flush=True)
                subprocess.run(command, env=environment, check=True)
                shell = SHELL.replace("__TITLE__", TITLES[name])
                shell = shell.replace("__CONTROLS__", "Snake. Arrow keys or WASD steer; Space or Enter restarts after a crash; Escape stops." if name == "snake" else "Particle Observatory. Space pauses or resumes; R restarts; S toggles stress mode.")
                shell = shell.replace("__ARGUMENTS__", "[]" if name == "snake" else "(matchMedia('(prefers-reduced-motion: reduce)').matches?['--paused']:[])")
                (directory / "run.html").write_text(shell)
                shutil.copyfile(ROOT / "LICENSE", directory / "LICENSE.txt")
                runtime_license = Path(compiler).resolve().parent / "LICENSE"
                if runtime_license.is_file():
                    shutil.copyfile(runtime_license, directory / "EMSCRIPTEN-LICENSE.txt")
                files = {path.name: digest(path.read_bytes()) for path in directory.iterdir() if path.is_file()}
                manifest = {"id": name, "source": str(source.relative_to(ROOT)), "header": DEMOS[name],
                            "peak_header": "Peak.h", "compiler": version, "flags": demo_flags(name),
                            "input_hash": input_hash(name), "files": files,
                            "presentation": ("Rend WebGL2; same Snake renderer/game source as native Vulkan" if name == "snake" else "Peak software window/backbuffer; same C source as native build")}
                (directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
            # Publish only after every requested build succeeds.
            for name in names:
                target = args.output / name
                target.mkdir(parents=True, exist_ok=True)
                for path in (staging / name).iterdir():
                    shutil.copyfile(path, target / path.name)
                print(f"built: {target}")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"build_web_demos: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
