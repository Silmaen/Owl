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

```c++
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
- **Warning**: `getRootPath()` infinite-loops if CWD is the project root; run via ctest, or run a test
  binary from the build directory

## Side effects

Tests **never write into the source tree**: use a temp directory (`std::filesystem::temp_directory_path()`)
and clean up. Fixtures that need GPU or audio must degrade to the Null backend in CI.

## Expensive Test Fixtures

For tests requiring slow one-time setup (e.g., a Slang compilation session), use `SetUpTestSuite`:
```c++
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

Inside Docker (see root `CLAUDE.md`), Clang presets only:
```bash
docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure -j8
docker/run.sh output/build/linux-clang-release/bin/owl_scene_tests_unit_test --gtest_filter='SceneHierarchy.*'
docker/run.sh poetry run python ci_action.py Test linux-clang-release
```
The whole suite runs in a few seconds once built (Release; debug + coverage is slower).

## Conventions

- Test names: `TEST(Module, Behavior)` — e.g., `TEST(Scene, CopyCreatesIndependentScene)`
- Existing categories (one folder each): core, debug, event, font, gui, input, io, layer, math, mesh, physics, renderer,
  scene, script, sound, voxel
- Tests link against both `OwlEngine` and `OwlEnginePrivate` (access to private headers)
- Timeout per test suite: 3600s (1 hour)
