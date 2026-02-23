---
name: coverage
description: Run code coverage analysis for the Owl project
---

# Generate code coverage

1. Coverage requires a **debug** preset (e.g., `linux-gcc-debug`, `linux-clang-debug`).
2. Use the CI system:
   ```bash
   poetry run python ci_action.py Coverage <debug-preset>
   ```
3. Or manually:
   - Build with debug preset (coverage flags are auto-enabled in debug)
   - Run tests via ctest
   - Generate report: `poetry run gcovr -r . -o output/build/<preset>/Coverage/index.html`
4. Report the coverage summary and location of the HTML report.
