#!/usr/bin/env python3
"""Capture a deterministic native frame from the Particle Observatory renderer.

The temporary C harness includes the current demo translation unit, initializes
its real particle state, advances 120 fixed steps, and writes the shared 720x420
software framebuffer. It does not open a Peak window or alter the demo source.
Requires a native C compiler and libm; no image library is needed.
"""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "site/static/images"
WIDTH, HEIGHT = 720, 420


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=OUTPUT, help="thumbnail output directory")
    args = parser.parse_args()
    harness = r'''#define main grit_demo_main
#include "demos/grit/demo.c"
#undef main
int main(void) {
    static uint32_t frame[WIDTH * HEIGHT];
    loop.view = (View){frame, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT};
    loop.reset = reset; loop.step = step; loop.render = render; loop.destroy = destroy;
    if (!loop.reset()) return 2;
    for (unsigned i = 0; i < 120; ++i)
        if (!loop.step((double)(i + 1) / 60.0)) return 3;
    if (!loop.render(&loop.view)) return 4;
    int ok = fwrite(frame, sizeof frame, 1, stdout) == 1;
    loop.destroy();
    return ok ? 0 : 5;
}
'''
    with tempfile.TemporaryDirectory(prefix="godstack-grit-capture-") as directory:
        directory = Path(directory)
        source, binary = directory / "capture.c", directory / "capture"
        source.write_text(harness)
        subprocess.run(["cc", "-std=c99", "-O2", "-I", str(ROOT), str(source), "-lm", "-o", str(binary)], check=True)
        raw = subprocess.run([str(binary)], check=True, capture_output=True, timeout=30).stdout
    if len(raw) != WIDTH * HEIGHT * 4:
        raise SystemExit(f"renderer wrote {len(raw)} bytes; expected {WIDTH * HEIGHT * 4}")
    pixels = struct.unpack(f"={WIDTH * HEIGHT}I", raw)
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)
        for pixel in pixels[y * WIDTH:(y + 1) * WIDTH]:
            rows.extend(((pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255, (pixel >> 24) & 255))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b""))
    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / "grit-demo.png"
    destination.write_bytes(png)
    print(f"Captured {destination} from the native 720x420 Particle Observatory renderer after 120 simulation steps.")


if __name__ == "__main__":
    main()
