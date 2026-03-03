---
paths:
  - "test/**/*.cpp"
  - "test/**/*.h"
---

# Testing Conventions (Google Test)

## Adding a New Test

1. Place `.cpp` file in `test/<category>_tests/` (e.g., `test/scene_tests/mytest.cpp`)
2. **No CMakeLists.txt edits required** — tests are auto-discovered by `file(GLOB_RECURSE ...)`
3. The test executable is automatically named `owl_<folder>_unit_test` (e.g., `owl_scene_tests_unit_test`)

## Test File Structure

```cpp
/**
 * @file MyTest.cpp
 * @author Silmaen
 * @date DD/MM/YYYY
 * ...
 */

#include "testHelper.h"

#include <gtest/gtest.h>
// other includes...

using namespace owl;

TEST(CategoryName, TestName) {
    // test body
}
```

## Test Helper

- Include `"testHelper.h"` from `test/test_helper/` (auto-included via CMake)
- `getRootPath()` returns the project root directory for locating test fixtures
- **Warning**: `getRootPath()` infinite-loops if CWD is the project root; always run via ctest

## Expensive Test Fixtures

For tests requiring slow one-time setup (e.g., Slang shader compilation ~50s), use `SetUpTestSuite`:
```cpp
class MyFixture : public testing::Test {
protected:
    static void SetUpTestSuite() {
        // expensive one-time init
    }
    static void TearDownTestSuite() {
        // cleanup
    }
};

TEST_F(MyFixture, TestName) { ... }
```

## Running Tests

```bash
ctest --test-dir output/build/<preset> --output-on-failure
```
Or via CI:
```bash
poetry run python ci_action.py Test <preset>
```

## Conventions

- Test names: `TEST(Module, Behavior)` — e.g., `TEST(Scene, CopyCreatesIndependentScene)`
- Follow existing test categories: core, debug, event, font, gui, input, layer, math, mesh, physic, renderer, scene, sound
- Tests link against both `OwlEngine` and `OwlEnginePrivate` (access to private headers)
- Timeout per test suite: 3600s (1 hour)
