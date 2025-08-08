/**
 * @file expected.h
 * @author Silmaen
 * @date 22/07/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl {
#ifdef __cpp_lib_expected
#include <expected>
using std::expected;
}
#else
#include <zeus/expected.hpp>

using zeus::expected;
using zeus::unexpected;

#endif

}// namespace owl
