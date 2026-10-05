# Ongoing quality commitments {#page-design-ongoing-quality}

[TOC]

Detail of the *Ongoing across all releases* section of the [Roadmap](../roadmap.md). These cross-cutting efforts are
never "done" — they are maintained and improved continuously across every release. No feature should regress the
baseline on these axes; each release is expected to move the needle forward. The rules behind them live in
`.claude/rules/ongoing-quality.md`.

## Render-style mixing

- Every new rendering mode ships as a renderer-stack layer that composes with the existing ones (2D, raycast, voxel,
  isometric, 3D) and with the HUD, in the editor and in Play
- The sample project keeps at least one scene mixing two or more rendering styles

## Code quality

- Keep clang-tidy / clang-format clean (no new warnings, no `// NOLINT` without justification)
- Refactor away duplication and dead code as it appears (no abstractions for hypothetical needs)
- Respect the conventions in `.claude/rules/cpp-style.md` (naming, trailing return types, smart-pointer aliases,
  `@brief` on every public API)
- Treat every PR as an opportunity to leave the touched files cleaner than found

## Test coverage

- Grow coverage alongside every new feature (no new public API without tests)
- Maintain coverage trend upward, never downward — use `poetry run python ci_action.py Coverage <preset>` to measure
- Unit tests for pure logic, integration tests for systems that cross module boundaries
- Fill gaps in existing modules opportunistically (backfill tests for untested legacy paths)
- Every bug fix lands with its regression test

## Performance

- Profile before optimizing — measure with Tracy, the frame bench or the `bench/` harness
- Watch for regressions in hot paths (renderer, physics step, scene update, script tick)
- Prefer algorithmic wins over micro-optimizations; document non-obvious perf tradeoffs
- Memory: avoid per-frame allocations, reuse buffers, stream large assets

## Performance budgets enforced in CI

- The benchmark suite runs in CI against a stored baseline; a regression beyond the threshold fails the build
- Each release that adds a hot path adds its benchmark and its budget

## Documentation quality

- Every public class, method, enum value, and struct field has a `@brief` / `///` comment
- Private members get at least a `///` one-liner
- Keep `doc/pages/*.md` in sync with behaviour — update pages in the same PR as the feature
- Prefer mermaid diagrams over ASCII art or external images for architecture/flow/sequence
- Update [doc/pages/changelog.md](../changelog.md) (Unreleased section) and the roadmap as features land

## Editor coverage for every authored object

Any new object the engine lets the user author (component, asset, sub-object inside a component) ships with full
editor support in Owl Nest **in the same PR** as the engine-side feature. "Editor support" means, at minimum:

- **Selection** — clickable in the viewport / hierarchy / asset browser (whichever is the natural surface for the
  object), with visible highlight
- **Inspection** — read every property in the inspector panel
- **Editing** — modify every property from the inspector, undo/redo via `ModifyEntityCommand` (or the
  asset-equivalent)
- **Bulk operations** — when the object is part of a collection (tilemap cells, animation keyframes, node-graph nodes,
  …), at least *select-many* + *move-group* / *delete-group*; resizing a parent must preserve / shift contents instead
  of clipping
- **Discoverability** — context-menu / drag-drop / keyboard shortcut where comparable objects already have one

"Working in YAML or via Lua only" is **not** acceptable for any object the user is expected to author by hand; treat
that gap as a regression of the same severity as a missing test or missing public API doc.
