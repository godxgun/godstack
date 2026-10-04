#!/usr/bin/env python3
"""Validate compiled Rend shader layouts and repeat its native workload."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
CLIENT = ROOT / "bin/rend_abi_vk"
STAGES = ("compute", "consume", "vertex", "fragment")


def run(args):
    process = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60)
    if process.returncode:
        raise RuntimeError(f"unexpected exit {process.returncode}: {' '.join(map(str, args))}\n"
                           f"{process.stdout}\n{process.stderr}")
    return process


def check_layout(host, reflection):
    params = reflection["parameters"]
    if len(params) != 1 or params[0]["name"] != "root" or params[0]["binding"]["index"] != 0:
        raise ValueError("shader must expose only root at binding zero")
    root = params[0]["type"]["elementType"]
    size = next(size for size in root["sizes"] if size["kind"] == "uniform")
    if (size["value"], size["alignment"]) != (host["size"], host["alignment"]):
        raise ValueError("host/shader root size or alignment mismatch")
    fields = {field["name"]: field for field in root["fields"]}
    if fields.keys() != host["fields"].keys():
        raise ValueError("host/shader fields mismatch")
    for name, layout in host["fields"].items():
        field = fields[name]
        binding = field["binding"]
        if (binding["offset"], binding["size"]) != (layout["offset"], layout["size"]):
            raise ValueError(f"host/shader field layout mismatch: {name}")
        kind = field["type"]["kind"]
        if kind == "pointer" and binding["size"] != host["pointer_size"]:
            raise ValueError(f"pointer width mismatch: {name}")
        if kind == "scalar" and field["type"]["scalarType"] != "uint32":
            raise ValueError(f"scalar ABI mismatch: {name}")
        if name == "vector" and (kind != "vector" or field["type"]["elementCount"] != 3
                                 or field["type"]["elementType"]["scalarType"] != "float32"):
            raise ValueError("vector ABI mismatch")
        if name == "matrix" and (kind != "matrix" or field["type"]["rowCount"] != 3
                                 or field["type"]["columnCount"] != 2
                                 or field["type"]["elementType"]["scalarType"] != "float32"):
            raise ValueError("matrix ABI mismatch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", type=int, default=0, help="explicit device index; no fallback")
    parser.add_argument("--runs", type=int, default=3, help="native workload repetitions")
    parser.add_argument("--layout-only", action="store_true", help="compiled host/shader layouts only; no GPU")
    args = parser.parse_args()
    if args.device < 0 or args.runs < 1:
        parser.error("device must be nonnegative and runs positive")
    host = json.loads(run([str(CLIENT), "--layout"]).stdout)
    for stage in STAGES:
        reflection = json.loads((ROOT / f"bin/rend_abi.{stage}.json").read_text())
        check_layout(host, reflection)
    print("rend_abi: four compiled host/shader layouts match", flush=True)
    if args.layout_only:
        return 0
    command = [str(CLIENT), "--device", str(args.device), "--profile", "graphics-compute"]
    for _ in range(args.runs):
        process = run(command)
        print(process.stdout, end="", flush=True)
        print(process.stderr, end="", file=sys.stderr)
    print("rend_abi: repeated native feasibility workload passed; native D3D12/Metal/task-mesh/presentation NOT TESTED")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"rend_abi: {error}", file=sys.stderr)
        sys.exit(1)
