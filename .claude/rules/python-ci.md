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

Available actions: Build, Test, Coverage, Clean, Documentation, Package, Help, DefineTeamCityVariables, PublishDoc, PublishPackage.

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
- Logging via `ci.log` (standard Python logging, auto-configured for TeamCity/Rich)
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
