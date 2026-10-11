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

ROOT = Path(__file__).resolve().parents[2]
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


def check_compiled_abi():
    for stage in ("vertex", "fragment"):
        reflection_path = ROOT / f"bin/snake.{stage}.json"
        reflection = json.loads(reflection_path.read_text())
        parameters = reflection.get("parameters", [])
        if len(parameters) != 1 or parameters[0].get("binding", {}).get("kind") != "pushConstantBuffer":
            raise ValueError(f"{stage} shader must reflect one push constant block")
        block = parameters[0]["type"]["elementType"]
        fields = block.get("fields", [])
        if len(fields) != 1 or fields[0].get("name") != "transform":
            raise ValueError(f"{stage} push block must contain only transform")
        matrix = fields[0]
        if (matrix["binding"].get("offset"), matrix["binding"].get("size")) != (0, 64):
            raise ValueError(f"{stage} matrix must occupy the 64-byte push block")
        if (matrix["type"].get("rowCount"), matrix["type"].get("columnCount")) != (4, 4):
            raise ValueError(f"{stage} transform must be a 4x4 matrix")
        if stage == "vertex":
            entry = next(entry for entry in reflection["entryPoints"] if entry["stage"] == "vertex")
            inputs = entry["parameters"][0]["type"]["fields"]
            actual = [(field["name"], field["binding"]["index"], field["type"].get("elementCount", 1))
                      for field in inputs]
            expected = [("position", 0, 3), ("shade", 1, 1), ("offset", 2, 3), ("scale", 3, 3), ("color", 4, 3)]
            if actual != expected:
                raise ValueError(f"vertex attributes do not match Snake buffers: {actual}")


def check_scene(image, frames):
    width, height, pixels = image
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
        raise ValueError(f"snake scene missing (colors={len(colors)}, "
                         f"non-clear={non_clear}, green={green}, red={red})")
    print(f"snake stress: {width}x{height}, deterministic {frames}-frame PPM; "
      f"{len(colors)} colors, {non_clear} non-clear pixels")

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
    check_compiled_abi()
    with tempfile.TemporaryDirectory(prefix="snake-render-") as directory:
        first = Path(directory) / "first.ppm"
        second = Path(directory) / "second.ppm"
        image_a = invoke(demo, args.frames, first)
        image_b = invoke(demo, args.frames, second)
        if image_a != image_b:
            raise ValueError("repeated fixed-frame render differs")
        check_scene(image_a, args.frames)
    print("snake stress: vertex/instance attributes and 64-byte matrix push block match generated reflection")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"snake stress: {error}", file=sys.stderr)
        sys.exit(1)
