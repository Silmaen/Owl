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

```cpp
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
    // TODO: implement test
}
```

4. No CMakeLists.txt changes needed — the test file is auto-discovered.
5. Build and run the test to verify:
   ```bash
   cmake --build output/build/linux-gcc-release --target owl_<category>_tests_unit_test
   output/build/linux-gcc-release/bin/owl_<category>_tests_unit_test --gtest_filter="<Category>.<TestName>"
   ```
6. Report the test result.
