#!/usr/bin/env python3
"""Compile and run the legacy Rend.h WebGL2 integration test in real Chromium.

Requires emcc, Node 22+, Python 3 and Chromium (or CHROMIUM=/path/to/browser).
All generated files and Emscripten cache writes stay in a temporary directory.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix='godstack-rend-web-') as name:
        directory = Path(name)
        environment = dict(os.environ, EM_CACHE=str(directory / 'cache'))
        cache = Path(os.environ.get('EM_CACHE', Path.home() / '.cache/emscripten'))
        if cache.is_dir():
            shutil.copytree(cache, directory / 'cache')
        subprocess.run([
            'emcc', '-std=c99', '-O2', '-I', str(ROOT),
            str(ROOT / 'tools/rend-webgl2-smoke/smoke.c'),
            '-sMIN_WEBGL_VERSION=2', '-sMAX_WEBGL_VERSION=2',
            '-sALLOW_MEMORY_GROWTH', '-sSTACK_SIZE=1048576', '-sEXIT_RUNTIME=0',
            '-sMODULARIZE', '-sEXPORT_NAME=createRendSmoke',
            '-sEXPORTED_FUNCTIONS=["_main","_rend_smoke_resize","_rend_smoke_finish"]',
            '-o', str(directory / 'rend-smoke.js'),
        ], env=environment, check=True)
        subprocess.run(['node', str(ROOT / 'tools/test_rend_browser.mjs'),
                        str(directory)], check=True)


if __name__ == '__main__':
    main()
