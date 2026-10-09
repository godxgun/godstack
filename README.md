```
 ▄▄ • ▄• ▄▌ ▐ ▄ .▄▄ · ▄▄▄▄▄ ▄▄▄·  ▄▄· ▄ •▄
▐█ ▀ ▪█▪██▌•█▌▐█▐█ ▀. •██  ▐█ ▀█ ▐█ ▌▪█▌▄▌▪
▄█ ▀█▄█▌▐█▌▐█▐▐▌▄▀▀▀█▄ ▐█.▪▄█▀▀█ ██ ▄▄▐▀▀▄·
▐█▄▪▐█▐█▄█▌██▐█▌▐█▄▪▐█ ▐█▌·▐█ ▪▐▌▐███▌▐█.█▌
·▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀▀  ▀▀▀  ▀  ▀ ·▀▀▀ ·▀  ▀
```

# Description

Collection of modular and minimal libraries used by our software.

These libraries are single-header files meant to be used as unity builds.

All files follow a simple format: 
1. Large "unboxing" comment explaining the library design and usage. 
2. Header guard followed by version, change log and public API.
3. Implementation MACRO and library implementation.
4. License.

# Libraries

## Core
| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Poof   | 0.2.1   | Build system.                              |
| Peak   | 1.0.0   | Single-header platform library with explicit runtime contexts. |
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

1. Copy and paste the header.
2. #define FOO_IMPLEMENTATION
3. Include foo.h.

## Build

```sh
cc build.c -o build  # bootstrap once
./build demo <name>  # build the selected demo
./build demo peak    # example
./build demo snake   # example 2
```

## Maturity & Versioning

Most libraries include a version and a change log at the top of the file.
This is helpful if you decide to update one of the libraries you are using in a project.
Libraries that have reached or surpassed the 1.0.0 landmark are generally considered
to be "completed."

## Documentation

Documentation can be found on [godgun.net](https://godgun.net/docs) (results may vary).
We recommend just reading the header files. It's all there.
