---
paths:
  - "source/owl/*/data/**"
  - "test/io_tests/**"
  - "test/mesh_tests/**"
  - "test/font_tests/**"
  - "test/voxel_tests/**"
---

# Data, assets & `.owlpack`

User-facing reference: `doc/pages/architecture.md`, `doc/pages/voxel.md`.

- `data::assets::pack` (`PackFormat`, `PackWriter`, `PackReader`, `PackExtractor`, `AssetScanner`) — not `io/`.
- A pack is untrusted input: `tryOpen` bounds offsets/sizes by the file size and `g_maxTocSize` /
  `g_maxEntrySize`, rejects unsafe paths (`isSafeEntryPath`), never throws; extraction goes through
  `extractPack` / `writeEntryFile` (confined, write failures reported). Fuzz target: `fuzz/` (`OWL_FUZZING`).
- Format: 40-byte header (magic `OWLP`), zstd-compressed blocks, XOR-obfuscated TOC at the end.
- `AssetScanner` walks scene YAML for `nam:` / `pat:` textures, fonts and teleport scene references
  (recursive).
- `SceneSerializer::deserializeFromBuffer()` loads a scene from memory (pack or network).
- `Application::openPack()` / `loadFromPack()` / `packContains()`; texture, font and script loaders try the
  open pack first, then the filesystem. Pack assets are extracted to `owl_pack_cache/` temp files.
- The runner opens the pack named by `PackFile` in `runner.yml`; Owl Nest *Project → Pack Game* writes one.
- `GameExporter` is the only export path (*Pack Game*, `OwlNest --export`, `export_tests`). Any asset reference a
  component adds must be followed by `AssetScanner`, or the end-to-end export test fails.
- A loader that produces an engine data type lives next to that type (`MeshLoader` in `data/geometry`).
