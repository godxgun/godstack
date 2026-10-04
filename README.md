```
 ▄▄ • ▄• ▄▌ ▐ ▄ .▄▄ · ▄▄▄▄▄ ▄▄▄·  ▄▄· ▄ •▄
▐█ ▀ ▪█▪██▌•█▌▐█▐█ ▀. •██  ▐█ ▀█ ▐█ ▌▪█▌▄▌▪
▄█ ▀█▄█▌▐█▌▐█▐▐▌▄▀▀▀█▄ ▐█.▪▄█▀▀█ ██ ▄▄▐▀▀▄·
▐█▄▪▐█▐█▄█▌██▐█▌▐█▄▪▐█ ▐█▌·▐█ ▪▐▌▐███▌▐█.█▌
·▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀▀  ▀▀▀  ▀  ▀ ·▀▀▀ ·▀  ▀
```

Collection of modular and minimal libraries used by our software.

## Core
| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Poof   | 0.2.1   | Build system.                              |
| Peak   | 0.12.0  | Platform library that automatically links the correct system libraries. |
| Fuse   | 0.13.0  | Immediate-mode UI command buffer.          |
| Rend   | 2.0.0   | Modern graphics API layer.                 |

## Utilities
| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Grit   | 0.2.1   | Allocators, math, rng, software raster.    |
| Cast   | 0.0.0   | C parser.                                  |

## Web Related
| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Cool   | 0.1.0   | HTML templating.                           |
| Wire   | 0.1.0   | HTTP server.                               |

## Installing

1. Copy and paste the folder.
2. -I the directory.
3. Include foo.h.
4. Include foo.c and it will pull other .c files as needed.

Poof is header-only: include `poof.h` before libc headers. It uses `#pragma once`
and static functions; no implementation macro or `.c` file is needed.

## Build

```
./build             # build demos, tools, and GPU artifacts
./build run         # run CPU workloads and headless GPU demos (requires GPU)
./build test        # backwards-compatible alias for demo runs, never a test suite
./build cpu         # build CPU-only Cast/Fuse/Grit/Peak demos
./build cpu run     # run CPU-only public-API workloads; no display or GPU
./build peak run    # run Peak headless workload without opening a window
./build rend run    # run offscreen Rend public-API workload
./build rend2 run   # run Rend2 public-API workload
./build rend abi run # repeat native Vulkan shader/ABI feasibility demo
./build package     # build package for github
python3 demos/package/demo.py # consume a relocated package through public APIs
./build tools       # build developer tools
./build snake       # build Rend2 snake demo
./build snake run   # repeated offscreen output and compiled shader reflection check
```

The default `run` workflow uses headless GPU demos and CPU workloads; it never launches interactive windows automatically. GPU workloads require suitable Vulkan hardware. `cpu` builds and runs Cast, Fuse, Grit, and Peak workloads without requiring a display or GPU at runtime. The Linux Peak demo uses Vulkan caller-backed placement and therefore still needs Vulkan development headers and loader linkage.

## Maturity & Versioning

Most libraries include a version and a change log at the top of the file.
This is helpful if you decide to update one of the libraries you are using in a project.
Libraries that have reached or surpassed the 1.0.0 landmark are generally considered
to be "completed."

## Documentation

Documentation can be found on [godgun.net](https://godgun.net/docs) (results may vary).
We recommend just reading the header files. It's all there.
