---
paths:
  - "**/*.py"
  - "pyproject.toml"
---

# Python & CI Conventions

## Environment

- **Always** use `poetry run` to execute Python commands, inside the build image:
  `docker/run.sh poetry run python ci_action.py …`. Never use `pip` or system Python.
- Install/sync: `poetry sync --no-root`
- Python version: >=3.12

## CI Entry Point

```bash
poetry run python ci_action.py <Action> <preset> [-v] [-q] [-- --extra=args]
```

Available actions: Build, Test, Coverage, Clean, Documentation, CodeStyle, ClangTidy, IncludeCheck, Bench, Fuzz,
Package, Help, DefineTeamCityVariables, PublishDoc, PublishPackage.

`Fuzz <preset>` runs every `owl_*_fuzzer` of a preset built with `OWL_FUZZING=ON` (`linux-fuzz`) for a fixed time;
any crash, leak or timeout fails it (see `doc/pages/continuous_integration.md#fuzzing`).

`IncludeCheck <preset>` configures a preset with `OWL_INCLUDE_CHECK=ON` (`linux-include-check`) and builds
`owl_include_check`: every header and source compiled alone, without PCH, against strict libc++.

`ClangTidy` (`--tool=tidy`, default, or `--tool=analyzer` for the `clang-analyzer-*`
checks only) drives clang-tidy from the build's `compile_commands.json` — the
compiler hook (`CMAKE_CXX_CLANG_TIDY`) is deliberately unset, because it cannot
skip a file. Requires the preset to be built first. On a pull request it
analyses only the translation units the diff can affect (touched `.cpp` files
plus every `.cpp` whose include closure, from `ninja -t deps`, contains a
touched header); anywhere else, or whenever the narrowing is not trustworthy, it
analyses all of them. Never make a doubtful case narrow the scope — falling back
to the full run is the only acceptable direction. Details:
`doc/pages/continuous_integration.md#clang-tidy-scoping`.

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
7. **std-includes** — every file under `source/`, `test/`, `bench/` includes the
   standard header of each `std::` symbol / `uint*_t` / `size_t` it names
   (`ci/utils/std_includes.py`; a `.cpp` may rely on its own header and `owlpch.h`).
8. **python** — `ruff check`, `ruff format --check`, `mypy` and `pytest ci/tests` on `ci/` and `ci_action.py` (configuration in
   `pyproject.toml`: 120 columns, rules `E F W I UP B SIM`); fix with `poetry run ruff check --fix` and
   `poetry run ruff format`.
9. **secrets** — git-tracked files scanned for private keys, GitHub / AWS /
   Slack tokens and passwords in URLs; a tracked `.env` fails. Prints the kind
   and position only, never the match.

Doxygen is **deliberately not** run here — the project already exposes a
separate `Documentation` action that builds doxygen with `WARN_AS_ERROR=YES`.

Each sub-check can be disabled with `-- --no-<name>=true`:
`--no-format`, `--no-typos`, `--no-comment-quality`, `--no-doc-audit`,
`--no-cpp-style`, `--no-structural`, `--no-std-includes`, `--no-python`, `--no-secrets`.

**Report findings through `_diag()`**, never `log.error()` directly. It prints
`<repo-relative path>:<line>:<column>: error: <check>: <message>` — the
GNU/clang diagnostic shape teamcity-github-bridge parses out of the build log to
annotate the pull request's diff. A finding printed in any other shape never
leaves the build log. Whole-file findings go on line 1; advisory ones pass
`level="warning"`.

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

- Type hints on all function signatures; the code passes `ruff` and `mypy` (CodeStyle `python` sub-check)
- Build output through `run_command(..., MODE_FOR_NINJA)`: only `: error:`, `FAILED:`, a stopped build and link
  errors are logged as errors, `: warning:` as warnings, the rest as information
- Use `pathlib.Path` for file paths (not string concatenation)
- Logging via `ci.log` (standard Python logging, autoconfigured for TeamCity/Rich)
- Return `int` exit codes from actions (0 = success)
- Use `from __future__ import annotations` if needed for forward refs
- Stateless actions: instantiated once, reused across calls

## Secrets

- **Never a secret in argv or in a log.** A password, token or key is read from an environment variable
  with `ci.utils.secrets.get_secret()` (which registers it for masking), never from an extra argument;
  refuse the old argument with `reject_secret_args()`. TeamCity provides it as an `env.*`
  parameter of the Global Build template (`common/Templates.kt`), referencing a server password parameter.
- Never hand a secret to a child process on its command line: call the library in process (`requests`) or pass it through the child's environment.
- Log commands only through `run_command` / `redact_command()`, never `' '.join(cmd)`; anything else that
  may echo a secret (server responses, exceptions) goes through `redact()`. Do not log a dict or object
  that holds a secret.
- Never download and execute code at CI time; publication uses the in-repository client in
  `ci/utils/publish.py` (HTTPS only).

## Tests

`docker/run.sh poetry run pytest` runs `ci/tests/`. Tests use fake values and mocks only (no network, no real
credential, no real publication) and write only to `tmp_path`.
