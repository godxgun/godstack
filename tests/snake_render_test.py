#!/usr/bin/env python3
"""Bounded deterministic offscreen smoke check for the snake demo.

Run after the snake target has built the existing demo executable. This checks
repeatability and visible output, not rendering performance or interactive UX.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DEMOS = (ROOT / "demos/snake/snake",)


def read_ppm(path):
    data = path.read_bytes()
    pos = 0

    def token():
        nonlocal pos
        while pos < len(data):
            if data[pos] == 35:  # comment
                while pos < len(data) and data[pos] not in (10, 13):
                    pos += 1
            elif data[pos] in b" \t\r\n":
                pos += 1
            else:
                break
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n#":
            pos += 1
        if start == pos:
            raise ValueError("truncated PPM header")
        return data[start:pos]

    if token() != b"P6":
        raise ValueError("expected binary P6 PPM")
    width, height, maximum = int(token()), int(token()), int(token())
    if width <= 0 or height <= 0 or maximum != 255:
        raise ValueError(f"invalid dimensions/range: {width}x{height}, max {maximum}")
    # PPM has one whitespace separator after maxval; consume exactly one byte
    # (CRLF is a single logical line ending).
    if pos >= len(data) or data[pos] not in b" \t\r\n":
        raise ValueError("missing PPM pixel separator")
    if data[pos:pos + 2] == b"\r\n":
        pos += 2
    else:
        pos += 1
    pixels = data[pos:]
    if len(pixels) != width * height * 3:
        raise ValueError(f"pixel payload length mismatch: {len(pixels)}")
    return width, height, pixels


def invoke(demo, frames, ppm):
    result = subprocess.run(
        [str(demo), "--headless", "--frames", str(frames), "--ppm", str(ppm)],
        cwd=ROOT, capture_output=True, text=True, timeout=120,
    )
    if result.returncode:
        raise RuntimeError(f"demo exited {result.returncode}\n{result.stdout}\n{result.stderr}")
    return read_ppm(ppm)


def check_shared_abi():
    header = (ROOT / "demos/snake/snake_gpu.h").read_text()
    shader = (ROOT / "demos/snake/shaders/snake.slang").read_text()
    if '#include "../snake_gpu.h"' not in shader:
        raise ValueError("Slang shader must consume the shared snake_gpu.h ABI")
    expected_assertions = (
        "snake_gpu_assert_vertex_size", "snake_gpu_assert_instance_size",
        "snake_gpu_assert_root_matrix", "snake_gpu_assert_root_vertices",
        "snake_gpu_assert_root_instances", "snake_gpu_assert_root_count",
        "snake_gpu_assert_root_size",
    )
    for assertion in expected_assertions:
        if assertion not in header:
            raise ValueError(f"missing host ABI assertion: {assertion}")
    for member in ("mvp", "vertices", "instances"):
        if f"root.{member}" not in shader:
            raise ValueError(f"Slang shader does not use root.{member}")

    expected = {"mvp": (0, 64), "vertices": (64, 8), "instances": (72, 8),
                "instance_count": (80, 4), "reserved": (84, 4)}
    for stage in ("vertex", "fragment"):
        reflection_path = ROOT / f"bin/snake.{stage}.json"
        if not reflection_path.is_file():
            raise ValueError(f"missing generated Slang reflection: {reflection_path}")
        reflection = json.loads(reflection_path.read_text())
        parameters = reflection.get("parameters", [])
        if len(parameters) != 1 or parameters[0].get("name") != "root":
            raise ValueError(f"{stage} shader must reflect a single root parameter")
        root = parameters[0]["type"]["elementType"]
        sizes = [item for item in root.get("sizes", []) if item.get("kind") == "uniform"]
        if not sizes or (sizes[0]["value"], sizes[0]["alignment"]) != (88, 8):
            raise ValueError(f"{stage} reflected root size/alignment mismatch")
        fields = {field["name"]: field for field in root.get("fields", [])}
        if set(fields) != set(expected):
            raise ValueError(f"{stage} reflected root fields mismatch: {set(fields)}")
        for name, layout in expected.items():
            binding = fields[name]["binding"]
            if (binding["offset"], binding["size"]) != layout:
                raise ValueError(f"{stage} root field layout mismatch: {name}")
        if fields["vertices"]["type"].get("kind") != "pointer" or fields["instances"]["type"].get("kind") != "pointer":
            raise ValueError(f"{stage} root pointer ABI mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--demo", type=Path, help="built demo executable; default: demos/snake/snake")
    parser.add_argument("--frames", type=int, default=4)
    args = parser.parse_args()
    candidates = (args.demo,) if args.demo else DEFAULT_DEMOS
    candidates = tuple(path if path.is_absolute() else ROOT / path for path in candidates)
    demo = next((path for path in candidates if path.is_file() and os.access(path, os.X_OK)), None)
    if args.frames < 1:
        parser.error("--frames must be positive")
    if demo is None:
        raise RuntimeError(f"built snake demo is missing or not executable: {', '.join(map(str, candidates))}")
    check_shared_abi()
    with tempfile.TemporaryDirectory(prefix="snake-render-") as directory:
        first = Path(directory) / "first.ppm"
        second = Path(directory) / "second.ppm"
        image_a = invoke(demo, args.frames, first)
        image_b = invoke(demo, args.frames, second)
        if image_a != image_b:
            raise ValueError("repeated fixed-frame render differs")
        width, height, pixels = image_a
        if (width, height) != (960, 720):
            raise ValueError(f"unexpected render extent: {width}x{height}")
        clear = (10, 13, 15)  # round(0.04, 0.05, 0.06) * 255
        colors = {tuple(pixels[i:i + 3]) for i in range(0, len(pixels), 3)}
        non_clear = sum(1 for i in range(0, len(pixels), 3)
                        if tuple(pixels[i:i + 3]) != clear)
        green = sum(1 for i in range(0, len(pixels), 3)
                    if pixels[i + 1] > 2 * pixels[i] and pixels[i + 1] > 2 * pixels[i + 2])
        red = sum(1 for i in range(0, len(pixels), 3)
                  if pixels[i] > 2 * pixels[i + 1] and pixels[i] > 2 * pixels[i + 2])
        if len(colors) < 10 or non_clear < 10000 or green < 100 or red < 100:
            raise ValueError(f"snake scene missing (colors={len(colors)}, non-clear={non_clear}, green={green}, red={red})")
    print(f"snake_render_test: {width}x{height}, deterministic {args.frames}-frame PPM; "
          f"{len(colors)} colors, {non_clear} non-clear pixels")
    print("snake_render_test: shared C/Slang root sizes, pointer widths, and offsets match generated reflection")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"snake_render_test: {error}", file=sys.stderr)
        sys.exit(1)
