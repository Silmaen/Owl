/**
 * @file CallValue.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace owl::script {

/// A value the host hands to a script function or reads from one: nil, a boolean, an integer, a number or a string.
using CallValue = std::variant<std::monostate, bool, int64_t, double, std::string>;

}// namespace owl::script
