# KalaGraphics

**KalaGraphics** is a C++20 window library for Windows and Linux, built for software and games with a user-friendly API that abstracts away Vulkan code as much as possible.

This project relies on several [external dependencies](https://github.com/greeenlaser/external-shared), they are not shipped inside this project, please make sure you have that repository cloned into a folder inside the same parent directory as this project folder before compiling this project from source.

---

## Import/Export

KalaGraphics supports (or plans to support) the following formats for its import and export pipeline. Formats prefixed with `k` are created and maintained by KalaKit.

The archive format is what the `k` prefixed formats can be imported or exported as, other external formats cannot be passed inside it.

textures:
import: png, ktex
export: ktex

meshes:
- import: gltf, kmod
- export: kmod

materials:
- import: kmat
- export: kmat

shaders:
- import: glsl, spv, ksha
- export: ksha

animations:
- import: kani
- export: kani

archive:
- import: kdat
- export: kdat

---

## Links

[Donate on PayPal](https://www.paypal.com/donate/?hosted_button_id=QWG8SAYX5TTP6)

[Official Discord server](https://discord.gg/BZ2eJ5Cdr8)

[Official Youtube channel](https://youtube.com/greenlaser)

---

## Docs

[How to build from source](docs/build_from_source.md)

[External libraries](docs/external_libraries.md)

[Minimum requirements](docs/minimum_requirements.md)

[Lost Empire Entertainment and KalaKit ecosystem](docs/ecosystem.md)
