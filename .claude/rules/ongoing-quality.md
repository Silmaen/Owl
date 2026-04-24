# Ongoing Quality Priorities

These are **cross-cutting efforts maintained across every release** — they are never "done" and no
feature should regress the baseline. Every PR is expected to move the needle forward on these axes
(or at minimum not backward).

Mirrors the *Ongoing across all releases* section of `doc/pages/roadmap.md`; keep both in sync.

## Code Quality

- Keep `clang-tidy` and `clang-format` clean — no new warnings, no `// NOLINT` without a comment
  explaining why the check is suppressed
- Follow `.claude/rules/cpp-style.md` strictly (naming, trailing return types, smart-pointer
  aliases, `@brief` on every public API element)
- Refactor duplication and dead code as it appears, but only what the current task justifies —
  do not introduce abstractions for hypothetical future needs
- Leave every touched file cleaner than you found it (naming, comments, unused includes)
- Prefer modern C++23 idioms: `std::optional`, structured bindings, `if constexpr`, `std::format`,
  init-statements in `if`, early returns, `constexpr`/`noexcept` where applicable

## Test Coverage

- **No new public API without tests** — add unit or integration tests alongside the feature, in
  the same PR
- Coverage trend must go up over time, never down — measure with
  `poetry run python ci_action.py Coverage linux-gcc-debug` (or your preferred preset)
- Unit tests for pure logic; integration tests for anything crossing module boundaries (scene +
  renderer, script + physics, pack + loader, etc.)
- Opportunistically backfill tests for untested legacy paths when you touch them
- Follow `.claude/rules/testing.md` for placement and naming conventions

## Performance

- **Measure before optimizing** — use the in-editor profiler, `OWL_PROFILE_SCOPE`, or external
  tools; never optimize blind
- Watch for regressions in hot paths: renderer draw loop, physics step, scene update, script tick,
  asset loading
- Prefer algorithmic wins (better data layout, avoiding O(n²) scans, caching derived state) over
  micro-optimizations
- Avoid per-frame allocations — reuse buffers, pool short-lived objects, stream large assets
- Document any non-obvious performance tradeoff with a one-line comment explaining *why*

## Documentation Quality

- Every public class, method, enum value, and struct field has a `@brief` or `///` comment
- Function parameters documented with `@param[in]` / `@param[out]` / `@param[in,out]`; return
  values with `@return`
- Private members get at least a `///` one-liner describing intent
- Keep `doc/pages/*.md` in sync with behaviour — update the relevant page in the **same PR** as
  the code change, not a follow-up
- Prefer mermaid diagrams (```` ```mermaid ````) over ASCII art or external images for
  architecture, flow, or sequence diagrams
- Update `CHANGELOG.md` (Unreleased section) and `doc/pages/roadmap.md` as features land —
  roadmap items flip Planned → In Progress → Done in the PR that performs the work

## When Implementing a New Feature

Checklist to run through before considering a feature complete:

1. Code follows `.claude/rules/cpp-style.md` (naming, headers, smart-pointer aliases)
2. Every new public symbol has Doxygen documentation
3. Tests added for the new public API (unit + integration as appropriate)
4. Coverage did not drop on affected modules
5. `doc/pages/*.md` updated (existing page edited or new page added + linked)
6. `CHANGELOG.md` has an entry under `[Unreleased]`
7. `doc/pages/roadmap.md` entry flipped to `![Done][done]` if the feature was on the roadmap
8. No new clang-tidy warnings introduced
9. Performance of hot paths verified unchanged (or improved)
