# Stable release criteria {#page-design-stable-release}

[TOC]

Criteria of the 1.0.0 release. No new feature: the release only closes these criteria, each verified by CI or by a
shipped artefact.

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
    - OwlEngine published as a Conan package (`conan create`, `test/package/`)
    - A consumer test project builds and runs against the installed Conan package in CI
- Two showcase games authored only in the editor
    - One 2D game and one 3D game, built in Owl Nest without engine-side C++ (Lua and visual scripting only), each
      mixing at least two rendering styles
- Platforms: Linux and Windows flawless (build, editor, runner, packaging), Android and the Web, each exported from every Owl Nest host
    - Windows executables signed by the `Package` action (`ci/utils/signing.py`, off without a certificate): with
      a Certum open-source code-signing certificate
    - An exported Windows game carries its project icon in its executable (resource rewritten at export)
    - Settings, logs and caches stay in the application folder on every OS (nothing scattered in the user
      profile); a new version can import the settings of a previous one
- Crash reporter in Owl Nest and exported games ([Crash reporter](crash-reporter.md))
- Localisation ([Localisation](localisation.md)) and accessibility ([Accessibility](accessibility.md))
- Zero known crash: no open crash or data-loss bug; fuzzers on every file loader run in CI
- Performance budgets verified in CI on every target platform
- Complete user documentation
    - Tutorials from an empty project to a packaged game, for each rendering style
    - Complete Lua API reference and visual-scripting node reference, generated from the binding registry

## Hardening phase (0.9.x) {#stable-release-hardening}

- Test protocols and release checklists: export per target, editor tour, sample games, upgrade from a previous
  version, documentation; the maintainer runs them.
- Exit to 1.0: CI green everywhere (every configuration, every target smoke test) and the test protocols pass.

## Project upgrade assistant {#stable-release-upgrade}

Opening a project of an earlier version, Owl Nest offers the tested migration, after a backup of the project.
