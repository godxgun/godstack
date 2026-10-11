#!/usr/bin/env python3
"""Build and execute real Peak/Rend WebGL2 regression tests in Chromium.

Requires emcc, Node 22+, Python 3 and Chromium. --native-ppm optionally compares
with `demos/snake/snake --headless --frames 2 --ppm FILE` output.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-ppm', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='godstack-snake-test-') as name:
        directory = Path(name)
        environment = dict(os.environ, EM_CACHE=str(directory / 'cache'))
        cache = Path(os.environ.get('EM_CACHE', Path.home() / '.cache/emscripten'))
        if cache.is_dir():
            shutil.copytree(cache, directory / 'cache')
        common = ['emcc', '-std=c99', '-O2', '-I', str(ROOT),
                  '-sMIN_WEBGL_VERSION=2', '-sMAX_WEBGL_VERSION=2']
        subprocess.run([*common, str(ROOT / 'demos/snake/snake.c'), '-lm',
                        '-DSNAKE_BROWSER_TEST', '-sASYNCIFY', '-sALLOW_MEMORY_GROWTH',
                        '-sSTACK_SIZE=1048576', '-sMODULARIZE', '-sEXPORT_NAME=createDemo',
                        '-sEXIT_RUNTIME=0', '-sEXPORTED_FUNCTIONS=["_main","_snake_test_state","_snake_test_resize"]',
                        '-sEXPORTED_RUNTIME_METHODS=["FS"]', '-o', str(directory / 'snake-test.js')],
                       env=environment, check=True)
        subprocess.run(['node', str(ROOT / 'tools/test_snake_browser.mjs'), str(directory),
                        *([str(args.native_ppm.resolve())] if args.native_ppm else [])], check=True)


if __name__ == '__main__':
    main()
