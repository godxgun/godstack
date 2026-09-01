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
| Poof   | 0.2.0   | Build system.                              |
| Peak   | 0.10.8  | Platform library that automatically links the correct system libraries. |
| Fuse   | 0.8.0   | Immediate-mode UI command buffer.          |
| Rend   | 1.6.5   | Modern graphics API layer.                 |

## Utilities
| Name   | Version | Description                                |
| ------ | ------- | ------------------------------------------ |
| Grit   | 0.2.1   | Allocators, math, rng, software raster.    |
| Cast   | 0.0.0   | C parser.                                  |
| Term   | 0.7.1   | Cell-grid terminal emulator as seen in [VT](https://github.com/valvesxyz/vt) |

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

## Build

```
./build             # demos + tests
./build test        # that, then run them headlessly
./build peak        # Peak demos + tests/peak
./build peak test   # that, then run tests/peak
```

Rend demos take `--headless` (`--frames N`, `--ppm path`). Peak window tests need a display (Xvfb on Linux CI).

## Maturity & Versioning

Most libraries include a version and a change log at the top of the file.
This is helpful if you decide to update one of the libraries you are using in a project.
Libraries that have reached or surpassed the 1.0.0 landmark are generally considered
to be "completed."

## Documentation

Documentation can be found on [godgun.net](https://godgun.net/docs) (results may vary).
We recommend just reading the header files. It's all there.
