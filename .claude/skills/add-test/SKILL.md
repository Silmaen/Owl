---
name: add-test
description: Create a new test file for the Owl engine
---

# Add a new test file

Arguments: `<category>` (e.g., scene, renderer, core) and `<test-name>`.

## Steps

1. Determine the test directory: `test/<category>_tests/`
2. If the directory doesn't exist, this is a new test category — create it.
3. Create the test file `test/<category>_tests/<test-name>_test.cpp` with this template:

```c++
/**
 * @file <test-name>_test.cpp
 * @author Silmaen
 * @date <DD/MM/YYYY>
 * Copyright (c) <YYYY> All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gtest/gtest.h>

using namespace owl;

TEST(<Category>, <TestName>) {
	// Arrange / act / assert on real behaviour, not a placeholder.
}
```

4. No CMakeLists.txt changes needed — the test file is auto-discovered.
5. Re-run configure (new file, GLOB), build and run the test to verify:
   ```bash
   docker/run.sh cmake --preset linux-clang-release -S .
   docker/run.sh cmake --build output/build/linux-clang-release --target owl_<category>_tests_unit_test
   docker/run.sh bash -c 'cd output/build/linux-clang-release && bin/owl_<category>_tests_unit_test --gtest_filter="<Category>.<TestName>"'
   ```
   Tests write only to a temp directory, never into the source tree.
6. Report the test result.
