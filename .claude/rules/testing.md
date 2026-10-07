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

## Image tests (`test/render_tests`, label `render`)

- One case = one scene of `test/render_tests/scenes/` rendered by `OwlRunner --frame-bench --capture` on lavapipe
  (Vulkan) or llvmpipe (OpenGL), compared to `test/render_tests/references/<backend>/<scene>.png` (RGB, 24 / 255
  per channel, 0.25 % of pixels). Captures, diffs and logs of failures go to `<build>/render_tests/`.
- Scenes have no Lua script, no physics and no input: the capture must not depend on timing.
- A deliberate visual change regenerates the references, reviewed image by image before committing:
  `docker/run.sh env OWL_RENDER_TESTS_UPDATE=1 ctest --test-dir output/build/linux-clang-release -L render`.
  This is the only command that writes into the source tree.
- CTest wraps the binary in `xvfb-run`; without a display or lavapipe the cases skip. About 8 s: run them in a
  CI stage of their own (`ctest -L render`, `-LE render` for the rest).

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

## Order independence and sanitizers

- Sanitizer presets set `OWL_TEST_SHUFFLE=ON`: every binary runs with `--gtest_shuffle`, so a test must not
  depend on another one's side effects (static state, environment variables, factory registrations). Reset
  what you touch, and never keep a pointer to a test-local object in static state.
- The seed is random and printed at the top of the binary's output (`Note: Randomizing tests' orders with a
  seed of N .`). Reproduce a failing order with the same seed:
  ```bash
  docker/run.sh env GTEST_RANDOM_SEED=N ctest --test-dir output/build/linux-sanitizer-address -R physics --output-on-failure
  docker/run.sh output/build/linux-sanitizer-address/bin/owl_physics_tests_unit_test --gtest_shuffle --gtest_random_seed=N
  ```
- ctest sets `ASAN_OPTIONS` / `UBSAN_OPTIONS` / `TSAN_OPTIONS` with `halt_on_error=1` (`test/CMakeLists.txt`), and
  the code is built with `-fno-sanitize-recover=all`: any sanitizer report fails the test. When running a binary
  by hand outside ctest, UBSan still aborts (no recovery compiled in).
- `test/tsan.supp` silences TSan only inside uninstrumented GPU code (lavapipe, LLVM, the validation layer);
  never add an Owl symbol there. Locally TSan needs ASLR off: `docker/run.sh --perf setarch -R ctest …`.

## Conventions

- Test names: `TEST(Module, Behavior)` — e.g., `TEST(Scene, CopyCreatesIndependentScene)`
- Existing categories (one folder each): core, debug, event, font, gui, input, io, layer, math, mesh, physics, render,
  renderer, runner, scene, script, sound, voxel, owlnest (editor: links the `OwlNestCore` static library, skipped when
  `OWL_BUILD_NEST` is off)
- Tests link against both `OwlEngine` and `OwlEnginePrivate` (access to private headers)
- Timeout per test suite: 3600s (1 hour)
