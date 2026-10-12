# Third-party notices and rights audit

Godstack's MIT License does not replace third-party licenses. Preserve their
notices, follow their terms, and assess the exact dependency versions and
linking/distribution mode. This is a repository inspection, not legal clearance
of every platform SDK or transitive dependency.

## Embedded code

- `Peak.h`: embedded Wayland xdg-shell protocol code carries the permissive
  notice naming Kristian Høgsberg, Rafael Antognolli, Jasper St. Pierre,
  Intel, Samsung, and Red Hat. That notice is preserved verbatim. These
  portions remain under their existing terms.
- `demos/teapot/stb_image.h`: Sean Barrett's 2017 MIT OR public-domain notice
  and other attribution remain untouched. Follow its chosen license.
- Historical `Type.h` at `8a204fc` embeds stb_truetype with Sean Barrett's
  2017 MIT OR public-domain notice. The pre-existing working-tree rewrite
  no longer includes that block. **Review required:** establish whether any
  derivatives survive and restore the applicable upstream attribution and
  terms if they do. Do not promise clearance of Type until this provenance
  review is complete. This licensing change does not remove or adjudicate
  those earlier permissions.
- `Fuse.h` cites Nuklear, Clay, and microui as design references; citations
  are retained. Confirm provenance of any copied implementation before
  claiming it as project-owned. References alone are not proof of copying.

## Assets: unresolved distribution blockers

| Material | Finding and required action |
| --- | --- |
| `demos/dashboard/Blue Screen Personal Use.ttf` | Filename indicates personal-use restriction; commit `fa6b192` also explicitly describes it as personal-use/not for commercial products. No full redistribution grant was found. Not covered by the MIT License; do not distribute without permission. Obtain a redistributable license, replace, or remove before release. |
| `demos/teapot/teapot.h` | Mesh data without a provenance/license notice. Not asserted to be project-owned and not relabeled. Confirm source and redistribution rights or replace/exclude. |
| `demos/teapot/wal.jpg` | Texture without a license/attribution record. Confirm source and redistribution rights or replace/exclude; no redistribution clearance asserted. |

The font's continued presence is a blocker to presenting an unfiltered
repository archive as wholly MIT-licensed or cleared for redistribution.
These files are left untouched pending the owner's decision, not cleared by
the root LICENSE. Dashboard and teapot source rights do not license their assets.

## Dependencies and local site/test material

The website is now external: `GODSTACK_SITE`, default `~/rsr/src/site`.
Rend2 and its checks are in the separate `~/rsr/src/godstack-pro` repository.

- Native builds use libc/platform APIs, X11, Wayland, Vulkan headers/loader,
  optional PulseAudio, zlib, and Apple/Windows SDK frameworks. Their licenses
  and SDK distribution terms are independent of Godstack. Runtime loading
  does not automatically remove licensing obligations. Inspect the selected
  backend and actual linked versions before shipping.
- Experimental `demos/type/shaping.c` uses HarfBuzz and FriBidi. HarfBuzz is
  generally permissively licensed; FriBidi is LGPL-licensed and may require
  notices/source and relinking rights depending on version/linking. Confirm
  exact terms; Godstack's MIT License does not waive LGPL duties.
- Shader builds use Slang; browser builds use Emscripten. Retain any runtime
  and bundled third-party notices. External-site `$GODSTACK_SITE/static/demos/*/EMSCRIPTEN-LICENSE.txt`
  is preserved. Prebuilt local browser artifacts keep their original MIT
  notices; do not relabel an old binary as a newly built artifact with
  different terms. Generated `$GODSTACK_SITE/public/` output is not edited.
- The untracked local website includes Iosevka fonts with
  `$GODSTACK_SITE/themes/godstack-heaven/static/fonts/Iosevka-LICENSE.md`; local test fonts
  have Liberation/Noto license files in `tests/fonts/`. Preserve those notices
  and review font bundling/modification conditions before distribution.
- Local site images and any other untracked assets lack a verified committed
  ownership trail in this audit; confirm provenance before release.

## Rights authority

Available history names Vasco/vasco, and existing notices name Vasco Alves.
No signed contributor grants were found. See [CONTRIBUTING.md](CONTRIBUTING.md)
for identity, employer, AI-assisted, and uncommitted-work checks. A Git author
name is not a chain-of-title guarantee. Unresolved rights must be cleared or
excluded before distribution. Earlier releases keep their own licenses.
