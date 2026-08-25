```
 ▄▄ • ▄• ▄▌ ▐ ▄ .▄▄ · ▄▄▄▄▄ ▄▄▄·  ▄▄· ▄ •▄
▐█ ▀ ▪█▪██▌•█▌▐█▐█ ▀. •██  ▐█ ▀█ ▐█ ▌▪█▌▄▌▪
▄█ ▀█▄█▌▐█▌▐█▐▐▌▄▀▀▀█▄ ▐█.▪▄█▀▀█ ██ ▄▄▐▀▀▄·
▐█▄▪▐█▐█▄█▌██▐█▌▐█▄▪▐█ ▐█▌·▐█ ▪▐▌▐███▌▐█.█▌
·▀▀▀▀  ▀▀▀ ▀▀ █▪ ▀▀▀▀  ▀▀▀  ▀  ▀ ·▀▀▀ ·▀  ▀
```

Collection of modular and minimal libraries used by our games. These libraries are designed to be
"vendored in" aka. copy-and-pasted into a project's folder. Some libraries may depend on each other,
but most can be used in isolation. Each library should contain a license in the header.

Use at your own risk.

| Name   | Description                                |
| ------ | ------------------------------------------ |
| Peak   | Single-header platform library that automatically links the write system libraries. |
| Grit   | Memory allocators, math, software raster.  |
| Poof   | Build software without going insane.       |
| Rend   | Modern graphics API layer.                 |
| Cool   | Render HTML by calling functions.          |

## Maturity & Versioning

Most libraries include a version and a change log at the top of the file.
This is helpful if you decide to update one of the libraries you are using in a project.
Libraries that have reached or surpassed the 1.0.0 landmark are generally considered
to be "completed."

## Documentation

Documentation can be found on [godgun.net](https://godgun.net/docs) (results may vary).
We recommend just reading the header files. It's all there.
