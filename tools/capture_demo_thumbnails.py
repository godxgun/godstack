#!/usr/bin/env python3
"""Capture gallery thumbnails from the real Particle Observatory and Snake demos.

Grit uses its native shared software renderer. Snake is a real native headless
Vulkan capture. Requires a C compiler and the native Snake build, Vulkan device,
and shaders. Does not build or install anything.
"""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

from site_paths import SITE

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = SITE / "static/images"


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png_from_ppm(data):
    magic, dimensions, maximum, pixels = data.split(b"\n", 3)
    if magic != b"P6" or maximum != b"255":
        raise ValueError("Expected the demo's 8-bit binary PPM output")
    width, height = map(int, dimensions.split())
    if len(pixels) != width * height * 3:
        raise ValueError("Incomplete captured raster")
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snake-ppm", type=Path, help="use an already captured real frame instead of running Snake")
    parser.add_argument("--output", type=Path, default=OUTPUT, help="thumbnail output directory")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    subprocess.run(["python3", str(ROOT / "tools/capture_grit_demo.py"), "--output", str(args.output.resolve())],
                   cwd=ROOT, check=True)

    with tempfile.TemporaryDirectory(prefix="godstack-thumbnails-") as temporary:
        ppm = args.snake_ppm
        if ppm is None:
            ppm = Path(temporary) / "snake.ppm"
            subprocess.run([str(ROOT / "demos/snake/snake"), "--headless", "--frames", "2", "--ppm", str(ppm)],
                           cwd=ROOT, check=True, timeout=30)
        (args.output / "snake-demo.png").write_bytes(png_from_ppm(ppm.read_bytes()))
        print(f"Captured {args.output / 'snake-demo.png'} from a real native Snake raster.")


if __name__ == "__main__":
    main()
