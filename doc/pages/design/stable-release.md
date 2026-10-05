# Stable release criteria {#page-design-stable-release}

[TOC]

Design page for v1.0.0, summarised in the [Roadmap](../roadmap.md). No new feature: the release only closes these
criteria, each verified by CI or by a shipped artefact.

## Criteria

- Public API frozen
    - Semantic versioning from 1.0.0 on; breaking changes only in a major version
    - Deprecation policy: a deprecated symbol stays for at least one minor release with a `[[deprecated]]` message
      pointing to its replacement
- Versioned scene and save formats with migrations
    - Every `.owl`, `.owlprefab`, `.owltilemap`, `.owltileset`, `.owl_save` file carries a format version; older files
      load through tested migration steps
- Consumable SDK
    - `find_package(OwlEngine)` pulls one or two public dependencies (EnTT, and imgui only through `Owl::Gui`)
    - A consumer test project builds and runs against the installed Conan package in CI
- Two showcase games authored only in the editor
    - One 2D game and one 3D game, built in Owl Nest without engine-side C++ (Lua and visual scripting only), each
      mixing at least two rendering styles
- Platforms: Linux and Windows flawless (build, editor, runner, packaging), plus the Web as third platform
- Zero known crash: no open crash or data-loss bug; fuzzers on every file loader run in CI
- Performance budgets verified in CI on every target platform
- Complete user documentation
    - Tutorials from an empty project to a packaged game, for each rendering style
    - Complete Lua API reference and visual-scripting node reference, generated from the binding registry
