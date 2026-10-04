#!/usr/bin/env python3
"""Check the native Rend feasibility client; not replacement/full conformance.

Run from godstack after ./build rend abi. No new test framework/dependencies.
Reflection is checked against the compiled C99 host, not copied offset constants.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
CLIENT = ROOT / "bin/rend_abi_vk"
STAGES = ("compute", "consume", "vertex", "fragment")


def run(args, success=True):
    process = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60)
    if (process.returncode == 0) != success:
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
    parser.add_argument("--layout-only", action="store_true", help="no GPU, compiled host/reflection only")
    args = parser.parse_args()
    if args.device < 0 or args.runs < 1:
        parser.error("device must be nonnegative and runs positive")
    host = json.loads(run([str(CLIENT), "--layout"]).stdout)
    for stage in STAGES:
        reflection = json.loads((ROOT / f"bin/rend_abi.{stage}.json").read_text())
        check_layout(host, reflection)
        # Verify that the checker rejects a demonstrated ABI mismatch.
        root = reflection["parameters"][0]["type"]["elementType"]
        root["fields"][0]["binding"]["offset"] += 4
        try:
            check_layout(host, reflection)
        except ValueError:
            pass
        else:
            raise ValueError("layout mismatch was not rejected")
    print("rend_abi: four compiled host/shader layouts match; mismatch rejection passed", flush=True)
    if args.layout_only:
        return 0
    command = [str(CLIENT), "--device", str(args.device), "--profile", "graphics-compute"]
    acquisitions = 0
    for _ in range(args.runs):
        process = run(command)
        print(process.stdout, end="", flush=True)
        print(process.stderr, end="", file=sys.stderr)
        acquisitions = int(re.search(r"native acquisition calls (\d+)", process.stdout)[1])
    for step in sorted({1, 3, 9, 16, 33, acquisitions}):
        process = run(command + ["--fail-create", str(step)])
        if "injected partial initialization failure cleaned up" not in process.stdout:
            raise RuntimeError(f"failure injection {step} was not validated")
        print(f"rend_abi: partial acquisition failure {step}: cleanup passed", flush=True)
    process = run([str(CLIENT), "--device", str(2**32 - 1), "--profile", "graphics-compute"], success=False)
    if "selected device" not in process.stderr or "unavailable" not in process.stderr:
        raise RuntimeError("invalid device was not rejected explicitly")
    process = run([str(CLIENT), "--device", str(args.device), "--profile", "full"], success=False)
    if "full profile rejected" not in process.stderr:
        raise RuntimeError("full profile was not rejected explicitly")
    print("rend_abi: device/full-profile rejection passed; no silent fallback")
    print("rend_abi: native feasibility checks passed; native D3D12/Metal/task-mesh/presentation NOT TESTED")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"rend_abi: {error}", file=sys.stderr)
        sys.exit(1)
