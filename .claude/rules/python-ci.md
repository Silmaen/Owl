---
paths:
  - "**/*.py"
  - "pyproject.toml"
---

# Python & CI Conventions

## Environment

- **Always** use `poetry run` to execute Python commands. Never use `pip` or system Python.
- Install/sync: `poetry sync --no-root`
- Python version: >=3.12

## CI Entry Point

```bash
poetry run python ci_action.py <Action> <preset> [-v] [-q] [-- --extra=args]
```

Available actions: Build, Test, Coverage, Clean, Documentation, CodeStyle, Package, Help, DefineTeamCityVariables,
PublishDoc, PublishPackage.

`CodeStyle` is the project's read-only style/doc gate. It **only inspects** —
it never rewrites sources. Sub-checks (all on by default):

1. **clang-format** dry-run on every C++ source.
2. **typos** via `codespell` (allowlist in `ci/codespell-ignore-words.txt`
   for legitimate identifiers like `nam:` / `siz:` prefixes).
3. **comment-quality** — `///` is reserved for single-line member/enum-value
   comments; consecutive `///` lines must use `/** */`. Each Doxygen
   description paragraph must end with `.`, `?`, or `!`. Implementation
   files (`.cpp` / `.cc` / `.cxx` / `.inl`) carry **only** the file-header
   Doxygen block — every additional `/** */` block, `///` or `///<` line,
   and multi-line `/* */` block must move to the matching header (the
   documentation belongs alongside the declaration, not duplicated).
   Single-line `//` comments inside function bodies are fine when the WHY
   is non-obvious.
4. **private-member-docs** — `m_*` / `mp_*` / `s_*` / `g_*` fields need a
   `///` line above or `///<` inline.
5. **cpp-style** — banned `std::shared_ptr` / `std::make_shared` /
   `std::unique_ptr` / `std::make_unique` / `std::weak_ptr` (project aliases:
   `shared` / `mkShared` / `uniq` / `mkUniq` / `weak`); banned class suffixes
   `*Service` / `*Helper` / `*Util`; `UI*` identifier prefix (must be `Ui*`);
   `enum class` (must be `enum struct`); blank-line rules around
   `OWL_PROFILE_FUNCTION()` and `OWL_DIAG_PUSH/POP`; log-message format
   (`Subsystem: capitalized message ending with .`).
6. **structural** — file headers (`@file` + `Copyright (c) YYYY`), `OWL_API`
   warnings for free functions declared in `source/owl/{public,private}`.

Doxygen is **deliberately not** run here — the project already exposes a
separate `Documentation` action that builds doxygen with `WARN_AS_ERROR=YES`.

Each sub-check can be disabled with `-- --no-<name>=true`:
`--no-format`, `--no-typos`, `--no-comment-quality`, `--no-doc-audit`,
`--no-cpp-style`, `--no-structural`.

## Adding a New CI Action

1. Create `ci/actions/myaction.py`
2. Define a class inheriting from `BaseAction`:

```python
from ci.actions.base.action import BaseAction
from ci.utils.preset import PresetConfig


class MyAction(BaseAction):
    def run(self, preset: PresetConfig, extra_args: list[str] | None = None) -> int:
        # return 0 for success, non-zero for failure
        return 0
```

3. Action is auto-discovered (no registration needed)

## Code Conventions

- Type hints on all function signatures
- Use `pathlib.Path` for file paths (not string concatenation)
- Logging via `ci.log` (standard Python logging, autoconfigured for TeamCity/Rich)
- Return `int` exit codes from actions (0 = success)
- Use `from __future__ import annotations` if needed for forward refs
- Stateless actions: instantiated once, reused across calls

## DepManager

Always through Poetry:

```bash
poetry run depmanager info version --raw
poetry run depmanager pack ls
poetry run depmanager info cmakedir --raw
```

Never call `depmanager` directly.
